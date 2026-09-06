#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <pwd.h>
#include <sys/types.h>
#include <limits.h>
#include <signal.h>
volatile sig_atomic_t timeout_occurred = 0;
void sigalrm_handler(int sig) {
    timeout_occurred = 1;
}
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include "lexer.h"
#include "parser.h"
#include "prompt.h"
#include "hop.h"
#include "reveal.h"
#include "peek.h"
#include "locate.h"
#include "exec_calls.h"
#include "redirection.h"
#include "pipe.h"
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 256
#endif
#define MAX_JOBS 1024
#define MAX_JOB_PROCS 256
struct job_proc {
    pid_t pid;
    char cmd[256];
    int alive; // 0=exited, 1=running, 2=stopped
};
struct bg_job {
    int id;
    pid_t pgid;
    int proc_count;
    struct job_proc procs[MAX_JOB_PROCS];
    char raw_cmd[256];
};
static struct bg_job jobs[MAX_JOBS];
static int job_count = 0;
static int next_job_id = 1;
static volatile sig_atomic_t fg_active = 0;
static void sigchld_handler(int sig)
{
    int saved_errno = errno;
    pid_t p;
    int status;
    while ((p = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        int job_idx = -1;
        int proc_idx = -1;
        for (int i = 0; i < job_count; i++) {
            for (int j = 0; j < jobs[i].proc_count; j++) {
                if (jobs[i].procs[j].pid == p) {
                    job_idx = i;
                    proc_idx = j;
                    break;
                }
            }
            if (job_idx != -1) break;
        }
        if (job_idx == -1) continue;

        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            jobs[job_idx].procs[proc_idx].alive = 0;
            char buf[512];
            int len = 0;
            if (WIFEXITED(status)) {
                len = snprintf(buf, sizeof(buf), "%s with pid %d exited normally\n", jobs[job_idx].procs[proc_idx].cmd, (int)p);
            } else if (WIFSIGNALED(status)) {
                len = snprintf(buf, sizeof(buf), "%s with pid %d exited abnormally\n", jobs[job_idx].procs[proc_idx].cmd, (int)p);
            }
            if (len > 0) {
                write(STDOUT_FILENO, buf, len);
            }
        } else if (WIFSTOPPED(status)) {
            jobs[job_idx].procs[proc_idx].alive = 2;
        } else if (WIFCONTINUED(status)) {
            jobs[job_idx].procs[proc_idx].alive = 1;
        }
    }
    errno = saved_errno;
}
static int is_builtin(char *cmd)
{
    if (strcmp(cmd, "hop")==0) return 1;
    if (strcmp(cmd, "reveal")==0) return 1;
    if (strcmp(cmd, "peek")==0) return 1;
    if (strcmp(cmd, "locate")==0) return 1;
    if (strcmp(cmd, "activities")==0) return 1;
    if (strcmp(cmd, "resume")==0) return 1;
    return 0;
}
void add_stopped_job(pid_t pgid, int proc_count, pid_t *pids, char cmds[][256], const char *raw_cmd) {
    if (job_count < MAX_JOBS) {
        jobs[job_count].id = next_job_id++;
        jobs[job_count].pgid = pgid;
        jobs[job_count].proc_count = proc_count;
        strncpy(jobs[job_count].raw_cmd, raw_cmd, 255);
        jobs[job_count].raw_cmd[255] = '\0';
        for (int i = 0; i < proc_count; i++) {
            jobs[job_count].procs[i].pid = pids[i];
            strncpy(jobs[job_count].procs[i].cmd, cmds[i], 255);
            jobs[job_count].procs[i].cmd[255] = '\0';
            jobs[job_count].procs[i].alive = 2; // stopped
        }
        job_count++;
        printf("[%d] + Stopped    %s\n", jobs[job_count-1].id, raw_cmd);
        fflush(stdout);
    }
}
void print_activities(void) {
    for (int i = 0; i < job_count; i++) {
        int all_exited = 1;
        for (int j = 0; j < jobs[i].proc_count; j++) {
            if (jobs[i].procs[j].alive) {
                all_exited = 0;
                break;
            }
        }
        if (!all_exited) {
            printf("[%d] pgid %d\n", jobs[i].id, (int)jobs[i].pgid);
            for (int j = 0; j < jobs[i].proc_count; j++) {
                if (jobs[i].procs[j].alive) {
                    const char *state = (jobs[i].procs[j].alive == 2) ? "Stopped" : "Running";
                    printf("  %d %s %s\n", (int)jobs[i].procs[j].pid, jobs[i].procs[j].cmd, state);
                }
            }
        }
    }
}

int execute_resume(char **args, int arg_count) {
    if (arg_count < 3 || args[1][0] != '%') {
        printf("resume: invalid syntax\n");
        return 0;
    }
    int target_id = atoi(args[1] + 1);
    int is_fg = -1;
    if (strcmp(args[2], "fg") == 0) is_fg = 1;
    else if (strcmp(args[2], "bg") == 0) is_fg = 0;
    
    if (is_fg == -1) {
        printf("resume: invalid syntax\n");
        return 0;
    }
    
    int timeout_sec = 0;
    if (arg_count > 3) {
        if (arg_count == 5 && strcmp(args[3], "--timeout") == 0 && is_fg == 1) {
            timeout_sec = atoi(args[4]);
            if (timeout_sec <= 0) {
                printf("resume: invalid syntax\n");
                return 0;
            }
        } else {
            printf("resume: invalid syntax\n");
            return 0;
        }
    }
    
    int job_idx = -1;
    for (int i = 0; i < job_count; i++) {
        // check if job is active (any process alive)
        int alive = 0;
        for (int j = 0; j < jobs[i].proc_count; j++) {
            if (jobs[i].procs[j].alive) alive = 1;
        }
        if (alive && jobs[i].id == target_id) {
            job_idx = i;
            break;
        }
    }
    if (job_idx == -1) {
        printf("resume: no such job\n");
        return 0;
    }
    
    // Construct command string for display
    char cmd_str[8192] = "";
    strncpy(cmd_str, jobs[job_idx].raw_cmd, 8191);
    
    kill(-jobs[job_idx].pgid, SIGCONT);
    
    if (is_fg == 0) { // bg
        for (int j = 0; j < jobs[job_idx].proc_count; j++) {
            if (jobs[job_idx].procs[j].alive == 2) jobs[job_idx].procs[j].alive = 1; // Mark Running
        }
        printf("[%d] + Running    %s\n", jobs[job_idx].id, cmd_str);
    } else { // fg
        printf("%s\n", cmd_str);
        
        tcsetpgrp(STDIN_FILENO, jobs[job_idx].pgid);
        
        if (timeout_sec > 0) {
            timeout_occurred = 0;
            alarm(timeout_sec);
        }
        
        int job_stopped = 0;
        for (int i = 0; i < jobs[job_idx].proc_count; i++) {
            if (jobs[job_idx].procs[i].alive) {
                jobs[job_idx].procs[i].alive = 1; // Mark Running while waiting
                int status;
                while (1) {
                    pid_t w = waitpid(jobs[job_idx].procs[i].pid, &status, WUNTRACED);
                    if (w == -1) {
                        if (errno == EINTR) {
                            if (timeout_sec > 0 && timeout_occurred) {
                                kill(-jobs[job_idx].pgid, SIGTERM);
                                printf("resume: job timed out\n");
                                timeout_occurred = 0;
                                // Wait, the process gets SIGTERM, so subsequent waitpid will reap it.
                            }
                            continue;
                        }
                        break;
                    }
                    if (WIFSTOPPED(status)) {
                        jobs[job_idx].procs[i].alive = 2; // stopped
                        job_stopped = 1;
                        break;
                    } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
                        jobs[job_idx].procs[i].alive = 0; // dead
                        break;
                    }
                }
            }
        }
        
        if (timeout_sec > 0) {
            alarm(0);
        }
        
        tcsetpgrp(STDIN_FILENO, getpgrp());
        
        if (job_stopped) {
            printf("[%d] + Stopped    %s\n", jobs[job_idx].id, cmd_str);
        }
    }
    return 0;
}
int execute_single(char **args, int arg_count, char *shell_home, char *prev_dir, pid_t *out_pid, int *stopped)
{
    int saved_stdin = dup(STDIN_FILENO);
    int saved_stdout = dup(STDOUT_FILENO);
    if (saved_stdin < 0 || saved_stdout < 0) {
        perror("dup");
        if (saved_stdin >= 0)
            close(saved_stdin);
        if (saved_stdout >= 0)
            close(saved_stdout);
        return 0;
    }
    char **clean_args = malloc(sizeof(char *) * (arg_count + 1));
    if (clean_args == NULL) {
        close(saved_stdin);
        close(saved_stdout);
        return 0;
    }
    int clean_count = 0;
    for (int i = 0; i < arg_count; i++) {
        if (strcmp(args[i], "<") == 0 || strcmp(args[i], ">") == 0 || strcmp(args[i], ">>") == 0) {
            i++;
            continue;
        }
        clean_args[clean_count++] = args[i];
    }
    clean_args[clean_count] = NULL;
    if (clean_count == 0) {
        close(saved_stdin);
        close(saved_stdout);
        free(clean_args);
        return 0;
    }
    int input_status = setup_input_redirection(args, arg_count);
    if (input_status == -1) {
        dup2(saved_stdin, STDIN_FILENO);
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdin);
        close(saved_stdout);
        free(clean_args);
        return 0;
    }
    FILE *output_tmp = NULL;
    int output_status = setup_output_redirection(args, arg_count, &output_tmp);
    if (output_status == -1) {
        dup2(saved_stdin, STDIN_FILENO);
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdin);
        close(saved_stdout);
        free(clean_args);
        return 0;
    }
    int ret = 0;
    fg_active = 1;
    if (strcmp(clean_args[0], "hop") == 0) {
        execute_hop(clean_args, clean_count, shell_home, prev_dir);
    } else if (strcmp(clean_args[0], "reveal") == 0) {
        execute_reveal(clean_args, clean_count, shell_home, prev_dir);
    } else if (strcmp(clean_args[0], "peek") == 0) {
        peek(clean_count, clean_args);
    } else if (strcmp(clean_args[0], "locate") == 0) {
        locate(clean_count, clean_args);
    } else if (strcmp(clean_args[0], "activities") == 0) {
        print_activities();
    } else if (strcmp(clean_args[0], "resume") == 0) {
        execute_resume(clean_args, clean_count);
    } else {
        ret = execute_external(clean_args, clean_count, out_pid, stopped);
    }
    fg_active = 0;
    if (dup2(saved_stdin, STDIN_FILENO) < 0) {
        perror("dup2");
    }
    if (dup2(saved_stdout, STDOUT_FILENO) < 0) {
        perror("dup2");
    }
    close(saved_stdin);
    close(saved_stdout);
    if (output_status == 1) {
        distribute_output(args, arg_count, output_tmp);
    }
    free(clean_args);
    return ret;
}
int launch_background_single(char **args, int arg_count, char *shell_home, char *prev_dir)
{
    char **clean_args = malloc(sizeof(char *) * (arg_count + 1));
    if (clean_args == NULL) return 0;
    int clean_count = 0;
    for (int i = 0; i < arg_count; i++) {
        if (strcmp(args[i], "<") == 0 || strcmp(args[i], ">") == 0 || strcmp(args[i], ">>") == 0) {
            i++;
            continue;
        }
        clean_args[clean_count++] = args[i];
    }
    clean_args[clean_count] = NULL;
    if (clean_count == 0) { free(clean_args); return 0; }
    if (is_builtin(clean_args[0])) {
        free(clean_args);
        return 0;
    }
    if (!check_external_exists(clean_args, clean_count)) {
        char *name = clean_args[0];
        if (name[0] == '%') name = name + 1;
        fprintf(stderr, "cshell: command not found (%s)\n", name);
        free(clean_args);
        return 1;
    }
    sigset_t mask, prev;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, &prev);
    pid_t pid = fork();
    if (pid > 0) {
        setpgid(pid, pid);
    }
    if (pid < 0) {
        perror("fork");
        sigprocmask(SIG_SETMASK, &prev, NULL);
        free(clean_args);
        return 0;
    }
    if (pid == 0) {
        setpgid(0, 0);
        sigprocmask(SIG_SETMASK, &prev, NULL);
        signal(SIGCHLD, SIG_DFL);

        int input_status = setup_input_redirection(args, arg_count);
        if (input_status == -1) _exit(1);
        FILE *output_tmp = NULL;
        int output_status = setup_output_redirection(args, arg_count, &output_tmp);
        if (output_status == -1) _exit(1);
        char *cmd = clean_args[0];
        if (strchr(cmd, '/') != NULL) {
            execv(cmd, clean_args);
            _exit(1);
        }
        char *lookup = cmd;
        if (cmd[0] == '%') { lookup = cmd + 1; clean_args[0] = lookup; cmd = lookup; }
        char local_path[4096];
        snprintf(local_path, sizeof(local_path), "./%s", cmd);
        if (access(local_path, X_OK) == 0) {
            execv(local_path, clean_args);
            _exit(1);
        }
        char *path = getenv("PATH");
        if (path != NULL) {
            char *pc = strdup(path);
            if (pc != NULL) {
                char *dir = strtok(pc, ":");
                while (dir != NULL) {
                    char fp[4096];
                    snprintf(fp, sizeof(fp), "%s/%s", dir, cmd);
                    if (access(fp, X_OK) == 0) {
                        execv(fp, clean_args);
                        _exit(1);
                    }
                    dir = strtok(NULL, ":");
                }
                free(pc);
            }
        }
        _exit(1);
    }
    if (job_count < MAX_JOBS) {
        jobs[job_count].id = next_job_id++;
        jobs[job_count].pgid = pid;
        jobs[job_count].proc_count = 1;
        jobs[job_count].procs[0].pid = pid;
        strncpy(jobs[job_count].procs[0].cmd, clean_args[0], sizeof(jobs[job_count].procs[0].cmd)-1);
        jobs[job_count].procs[0].cmd[sizeof(jobs[job_count].procs[0].cmd)-1] = '\0';
        if (jobs[job_count].procs[0].cmd[0] == '%') {
            memmove(jobs[job_count].procs[0].cmd, jobs[job_count].procs[0].cmd+1, strlen(jobs[job_count].procs[0].cmd));
        }
        char full_cmd[256] = {0};
        for(int k=0; k<arg_count; k++) {
            strncat(full_cmd, args[k], 255 - strlen(full_cmd));
            if (k < arg_count - 1) strncat(full_cmd, " ", 255 - strlen(full_cmd));
        }
        strncpy(jobs[job_count].raw_cmd, full_cmd, 255);
        jobs[job_count].raw_cmd[255] = '\0';
        jobs[job_count].procs[0].alive = 1;
        job_count++;
        printf("[%d] %d\n", jobs[job_count-1].id, (int)pid);
        fflush(stdout);
    }
    sigprocmask(SIG_SETMASK, &prev, NULL);
    free(clean_args);
    return 0;
}
int segment_has_pipe(token *start, token *end)
{
    token *t = start;
    while (t != end) {
        if (t->type == OP_PIPE)
            return 1;
        t = t->next_token;
    }
    return 0;
}
int build_line_from_segment(token *start, token *end, char *out, int out_size)
{
    int pos = 0;
    token *t = start;
    while (t != end) {
        int len = strlen(t->content);
        if (pos + len + 1 >= out_size)
            return -1;
        memcpy(out + pos, t->content, len);
        pos += len;
        t = t->next_token;
        if (t != end) {
            out[pos++] = ' ';
        }
    }
    out[pos] = '\0';
    return 0;
}

void dummy_handler(int sig) {
    if (sig == SIGINT || sig == SIGTSTP) {
        write(STDOUT_FILENO, "\n", 1);
    }
}
int main()
{
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // Don't use SA_RESTART so getline can be interrupted
    sigaction(SIGCHLD, &sa, NULL);

    struct sigaction sa_int;
    sa_int.sa_handler = dummy_handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    sigaction(SIGINT, &sa_int, NULL);
    sigaction(SIGTSTP, &sa_int, NULL);
    signal(SIGTTOU, SIG_IGN);
    struct sigaction sa_alrm;
    sa_alrm.sa_handler = sigalrm_handler;
    sigemptyset(&sa_alrm.sa_mask);
    sa_alrm.sa_flags = 0; // MUST be 0 to interrupt waitpid
    sigaction(SIGALRM, &sa_alrm, NULL);

    uid_t user_uid = getuid();
    struct passwd *pw = getpwuid(user_uid);
    const char *username = "unknown";
    if (pw == NULL) {
        fprintf(stderr, "shell : could not resolve username\n");
    } else {
        username = pw->pw_name;
    }
    char hostname[HOST_NAME_MAX];
    if (gethostname(hostname, HOST_NAME_MAX) != 0) {
        strcpy(hostname, "unknown");
    }
    char shell_home[4096];
    getcwd(shell_home, sizeof(shell_home));
    char *line = NULL;
    size_t len = 0;
    char prev_dir[4096] = "";
    int ctrl_d_count = 0;
    while (true) {
        display_prompt(username, hostname, shell_home);
        fflush(stdout);
        ssize_t nread = getline(&line, &len, stdin);
        if (nread == -1) {
            if (errno == EINTR) {
                clearerr(stdin);
                continue;
            }
            int has_stopped = 0;
            for (int i = 0; i < job_count; i++) {
                for (int j = 0; j < jobs[i].proc_count; j++) {
                    if (jobs[i].procs[j].alive == 2) has_stopped = 1;
                }
            }
            if (has_stopped && ctrl_d_count == 0) {
                printf("\ncshell: there are stopped jobs\n");
                fflush(stdout);
                clearerr(stdin);
                ctrl_d_count++;
                continue;
            }
            for (int i = 0; i < job_count; i++) {
                int group_alive = 0;
                for (int j = 0; j < jobs[i].proc_count; j++) {
                    if (jobs[i].procs[j].alive) group_alive = 1;
                }
                if (group_alive) {
                    kill(-jobs[i].pgid, SIGHUP);
                    kill(-jobs[i].pgid, SIGCONT); // Make sure they process the SIGHUP if stopped
                }
            }
            break;
        } else {
            if (nread > 1) { // Not just an empty newline
                ctrl_d_count = 0;
            } else if (nread == 1 && line[0] == '\n') {
                // If just enter is pressed, maybe reset count? "If Ctrl-D is pressed again immediately afterward (no other input in between)". So yes, entering empty line is input!
                ctrl_d_count = 0;
            }
        }
        line[strcspn(line, "\n")] = '\0';
        int ok = 1;
        token *token_head = lexer(line, &ok);
        if (ok == 0 || token_head == NULL) {
            if (token_head)
                free_tokens(token_head);
            continue;
        }
        int grammar_check = parser(token_head);
        if (grammar_check == 1) {
            fprintf(stderr, "cshell: invalid syntax\n");
            free_tokens(token_head);
            continue;
        }
        token *seg_start = token_head;
        token *cur = token_head;
        int should_break_outer = 0;
        while (true) {
            int is_end = (cur == NULL);
            int is_delim = 0;
            int is_bg = 0;
            if (!is_end && (cur->type == OP_SEMI || cur->type == OP_AMP)) {
                is_delim = 1;
                if (cur->type == OP_AMP) is_bg = 1;
            }
            if (is_end || is_delim) {
                token *seg_end = cur;
                int count = 0;
                token *tmp = seg_start;
                while (tmp != seg_end) {
                    count++;
                    tmp = tmp->next_token;
                }
                if (count > 0) {
                    if (segment_has_pipe(seg_start, seg_end)) {
                        char pipe_line[8192];
                        if (build_line_from_segment(seg_start, seg_end, pipe_line, sizeof(pipe_line)) == 0) {
                            if (is_bg) {
                                sigset_t mask, prev;
                                sigemptyset(&mask);
                                sigaddset(&mask, SIGCHLD);
                                sigprocmask(SIG_BLOCK, &mask, &prev);
                                pid_t out_pids[MAX_PIPE_COMMANDS];
                                char out_cmds[MAX_PIPE_COMMANDS][256];
                                int num_cmds = execute_pipeline_bg(pipe_line, shell_home, prev_dir, out_pids, out_cmds);
                                if (num_cmds > 0) {
                                    if (job_count < MAX_JOBS) {
                                        jobs[job_count].id = next_job_id++;
                                        jobs[job_count].pgid = out_pids[0];
                                        jobs[job_count].proc_count = num_cmds;
                                        for (int i = 0; i < num_cmds; i++) {
                                            jobs[job_count].procs[i].pid = out_pids[i];
                                            strncpy(jobs[job_count].procs[i].cmd, out_cmds[i], 255);
                                            jobs[job_count].procs[i].cmd[255] = '\0';
                                            if (jobs[job_count].procs[i].cmd[0] == '%') {
                                                memmove(jobs[job_count].procs[i].cmd, jobs[job_count].procs[i].cmd+1, strlen(jobs[job_count].procs[i].cmd));
                                            }
                                            jobs[job_count].procs[i].alive = 1;
                                        }
                                        strncpy(jobs[job_count].raw_cmd, pipe_line, 255);
                                        jobs[job_count].raw_cmd[255] = '\0';
                                        job_count++;
                                        printf("[%d] %d\n", jobs[job_count-1].id, (int)out_pids[0]);
                                        fflush(stdout);
                                    }
                                }
                                sigprocmask(SIG_SETMASK, &prev, NULL);
                            } else {
                                fg_active = 1;
                                sigset_t mask, prev;
                                sigemptyset(&mask);
                                sigaddset(&mask, SIGCHLD);
                                sigprocmask(SIG_BLOCK, &mask, &prev);
                                pid_t out_pids[MAX_PIPE_COMMANDS];
                                char out_cmds[MAX_PIPE_COMMANDS][256];
                                int stopped = 0;
                                int num_cmds = execute_pipeline(pipe_line, shell_home, prev_dir, out_pids, out_cmds, &stopped);
                                sigprocmask(SIG_SETMASK, &prev, NULL);
                                fg_active = 0;
                                if (stopped && num_cmds > 0) {
                                    add_stopped_job(out_pids[0], num_cmds, out_pids, out_cmds, pipe_line);
                                }
                            }
                        }
                    } else {
                        char **args = malloc(sizeof(char *) * (count + 1));
                        if (args != NULL) {
                            tmp = seg_start;
                            for (int i = 0; i < count; i++) {
                                args[i] = tmp->content;
                                tmp = tmp->next_token;
                            }
                            args[count] = NULL;
                            int st = 0;
                            if (is_bg) {
                                st = launch_background_single(args, count, shell_home, prev_dir);
                            } else {
                                pid_t single_pid = 0;
                                int stopped = 0;
                                st = execute_single(args, count, shell_home, prev_dir, &single_pid, &stopped);
                                if (stopped && single_pid > 0) {
                                    pid_t pids[1] = {single_pid};
                                    char cmds[1][256];
                                    strncpy(cmds[0], args[0], 255);
                                    cmds[0][255] = '\0';
                                    char raw_cmd[256] = {0};
                                    for(int k=0; k<count; k++) {
                                        strncat(raw_cmd, args[k], 255 - strlen(raw_cmd));
                                        if (k < count - 1) strncat(raw_cmd, " ", 255 - strlen(raw_cmd));
                                    }
                                    add_stopped_job(single_pid, 1, pids, cmds, raw_cmd);
                                }
                                if (st == 1) {
                                    free(args);
                                    should_break_outer = 1;
                                }
                            }
                            free(args);
                            if (should_break_outer) break;
                        }
                    }
                }
                if (is_end)
                    break;
                seg_start = cur->next_token;
                if (should_break_outer) break;
            }
            if (cur == NULL)
                break;
            cur = cur->next_token;
        }
        free_tokens(token_head);
    }
    free(line);
    return 0;
}
