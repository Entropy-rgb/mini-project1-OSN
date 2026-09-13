#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>
#include "snoop.h"
#include "sys_table.h"
#include "exec_calls.h"

struct syscall_stat {
    int sys_num;
    int count;
    double total_time;
    int first_occ;
};

static int cmp_sys(const void *a, const void *b) {
    const struct syscall_stat *sa = a;
    const struct syscall_stat *sb = b;
    if (sa->count != sb->count) {
        return sb->count - sa->count; // descending
    }
    return sa->first_occ - sb->first_occ; // ascending order of appearance
}

int execute_snoop(char **args, int arg_count, char *shell_home, char *prev_dir) {
    if (arg_count < 2) {
        printf("snoop: invalid syntax\n");
        return 0;
    }
    
    int is_attach = 0;
    pid_t target_pid = -1;
    if (strcmp(args[1], "-p") == 0) {
        if (arg_count != 3) {
            printf("snoop: invalid syntax\n");
            return 0;
        }
        is_attach = 1;
        target_pid = atoi(args[2]);
        if (kill(target_pid, 0) == -1) {
            printf("snoop: no such process\n");
            return 0;
        }
    } else {
        if (!check_external_exists(&args[1], arg_count - 1)) {
            printf("snoop: command not found\n");
            return 0;
        }
    }
    
    pid_t pid;
    if (is_attach) {
        pid = target_pid;
        if (ptrace(PTRACE_ATTACH, pid, 0, 0) == -1) {
            printf("snoop: no such process\n");
            return 0;
        }
    } else {
        pid = fork();
        if (pid < 0) {
            perror("fork");
            return 0;
        }
        if (pid == 0) {
            ptrace(PTRACE_TRACEME, 0, 0, 0);
            raise(SIGSTOP); // Stop ourselves to give parent time to setup
            
            char **exec_args = malloc(sizeof(char *) * arg_count);
            for (int i = 1; i < arg_count; i++) {
                exec_args[i-1] = args[i];
            }
            exec_args[arg_count-1] = NULL;
            
            execvp(exec_args[0], exec_args);
            perror("execvp");
            _exit(1);
        }
    }
    
    int status;
    waitpid(pid, &status, 0); // Wait for the initial stop (either from ATTACH or SIGSTOP)
    
    // Some processes might have already exited before we attach properly or if they get killed
    if (WIFEXITED(status) || WIFSIGNALED(status)) {
        printf("snoop: no such process\n");
        return 0;
    }

    ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_TRACESYSGOOD);
    
    struct syscall_stat stats[1024];
    for(int i=0; i<1024; i++) {
        stats[i].sys_num = i;
        stats[i].count = 0;
        stats[i].total_time = 0;
        stats[i].first_occ = 0;
    }
    
    int occ_counter = 0;
    int in_syscall = 0;
    
    struct timespec entry_time;
    
    int pass_sig = 0;
    while (1) {
        ptrace(PTRACE_SYSCALL, pid, 0, pass_sig);
        waitpid(pid, &status, 0);
        
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            break;
        }
        
        pass_sig = 0;
        if (WIFSTOPPED(status)) {
            int stopsig = WSTOPSIG(status);
            if (stopsig == (SIGTRAP | 0x80)) {
            struct user_regs_struct regs;
            ptrace(PTRACE_GETREGS, pid, 0, &regs);
            long orig_rax = regs.orig_rax;
            
            if (!in_syscall) {
                in_syscall = 1;
                clock_gettime(CLOCK_MONOTONIC, &entry_time);
                if (orig_rax >= 0 && orig_rax < 1024) {
                    if (stats[orig_rax].count == 0) {
                        stats[orig_rax].first_occ = ++occ_counter;
                    }
                    stats[orig_rax].count++;
                }
            } else {
                in_syscall = 0;
                struct timespec exit_time;
                clock_gettime(CLOCK_MONOTONIC, &exit_time);
                double elapsed = (exit_time.tv_sec - entry_time.tv_sec) + 
                                 (exit_time.tv_nsec - entry_time.tv_nsec) / 1e9;
                if (orig_rax >= 0 && orig_rax < 1024) {
                    stats[orig_rax].total_time += elapsed;
                }
            }
            } else if (stopsig != SIGTRAP) {
                pass_sig = stopsig;
            }
        }
    }
    
    // Sort and print
    qsort(stats, 1024, sizeof(struct syscall_stat), cmp_sys);
    
    printf("%-20s %-10s %s\n", "syscall", "calls", "time");
    for (int i = 0; i < 1024; i++) {
        if (stats[i].count > 0) {
            const char *name = syscall_names[stats[i].sys_num];
            char buf[64];
            if (!name) {
                sprintf(buf, "syscall_%d", stats[i].sys_num);
                name = buf;
            }
            printf("%-20s %-10d %.3fs\n", name, stats[i].count, stats[i].total_time);
        }
    }
    
    return 0;
}
