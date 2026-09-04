#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <pwd.h>
#include <sys/types.h>
#include <limits.h>
#include <signal.h>
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
    return 0;
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
int execute_single(char **args, int arg_count, char *shell_home, char *prev_dir)
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
    } else {
        ret = execute_external(clean_args, clean_count);
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
int main()
{
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // Don't use SA_RESTART so getline can be interrupted
    sigaction(SIGCHLD, &sa, NULL);
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
    while (true) {
        display_prompt(username, hostname, shell_home);
        fflush(stdout);
        ssize_t nread = getline(&line, &len, stdin);
        if (nread == -1) {
            if (errno == EINTR) {
                clearerr(stdin);
                continue;
            }
            break;
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
                                execute_pipeline(pipe_line, shell_home, prev_dir);
                                sigprocmask(SIG_SETMASK, &prev, NULL);
                                fg_active = 0;
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
                                st = execute_single(args, count, shell_home, prev_dir);
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
