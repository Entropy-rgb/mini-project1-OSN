#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include "lexer.h"
#include "parser.h"
#include "hop.h"
#include "reveal.h"
#include "peek.h"
#include "locate.h"
#include "spy.h"
#include "snoop.h"

#include <sys/stat.h>
static int is_regular_executable(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode) && (access(path, X_OK) == 0)) {
        return 1;
    }
    return 0;
}
#include "exec_calls.h"
#include "redirection.h"
#include "pipe.h"
static void execute_command(char **args,
                             int arg_count,
                             char *shell_home,
                             char *prev_dir)
{
    char **clean_args = malloc(sizeof(char *) * (arg_count + 1));
    if (clean_args == NULL)
        _exit(1);
    int clean_count = 0;
    for (int i = 0; i < arg_count; i++)
    {
        if (strcmp(args[i], "<") == 0 ||
            strcmp(args[i], ">") == 0 ||
            strcmp(args[i], ">>") == 0)
        {
            i++;
            continue;
        }
        clean_args[clean_count++] = args[i];
    }
    clean_args[clean_count] = NULL;
    int saved_stdout = dup(STDOUT_FILENO);
    if (saved_stdout < 0)
    {
        free(clean_args);
        _exit(1);
    }
    int input_status =
        setup_input_redirection(args, arg_count);
    if (input_status == -1)
    {
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
        free(clean_args);
        _exit(1);
    }
    FILE *output_tmp = NULL;
    int output_status =
        setup_output_redirection(args,
                                 arg_count,
                                 &output_tmp);
    if (output_status == -1)
    {
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
        free(clean_args);
        _exit(1);
    }
    if (clean_count == 0)
    {
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
        if (output_tmp != NULL)
            fclose(output_tmp);
        free(clean_args);
        _exit(1);
    }
    if (strcmp(clean_args[0], "hop") == 0)
    {
        execute_hop(clean_args,
                    clean_count,
                    shell_home,
                    prev_dir);
    }
    else if (strcmp(clean_args[0], "reveal") == 0)
    {
        execute_reveal(clean_args,
                       clean_count,
                       shell_home,
                       prev_dir);
    }
    else if (strcmp(clean_args[0], "peek") == 0)
    {
        peek(clean_count, clean_args);
    }
    else if (strcmp(clean_args[0], "locate") == 0)
    {
        locate(clean_count, clean_args);
    }    else if (strcmp(clean_args[0], "activities") == 0)
    {
        extern void print_activities(void);
        print_activities();
    }
    else if (strcmp(clean_args[0], "resume") == 0)
    {
        // Ignore in pipe or call execute_resume (not expected in pipe)
    }
    else if (strcmp(clean_args[0], "ping") == 0)
    {
        // Ignore in pipe
    }
    else if (strcmp(clean_args[0], "spy") == 0)
    {
        execute_spy(clean_args, clean_count);
    }
    else if (strcmp(clean_args[0], "snoop") == 0)
    {
        execute_snoop(clean_args, clean_count, shell_home, prev_dir);
    }
    else if (strcmp(clean_args[0], "resume") == 0)
    {
        // Ignore in pipe or call execute_resume (not expected in pipe)
    }
    else if (strcmp(clean_args[0], "ping") == 0)
    {
        // Ignore in pipe
    }
    else if (strcmp(clean_args[0], "spy") == 0)
    {
        execute_spy(clean_args, clean_count);
    }
    else if (strcmp(clean_args[0], "snoop") == 0)
    {
        execute_snoop(clean_args, clean_count, shell_home, prev_dir);
    }
    else
    {
        execute_external(clean_args, clean_count, NULL, NULL);
    }
    if (dup2(saved_stdout, STDOUT_FILENO) < 0)
    {
        close(saved_stdout);
        free(clean_args);
        _exit(1);
    }
    close(saved_stdout);
    if (output_status == 1)
    {
        distribute_output(args,
                          arg_count,
                          output_tmp);
    }
    free(clean_args);
    _exit(0);
}
void execute_command_bg(char **args,
                             int arg_count,
                             char *shell_home,
                             char *prev_dir)
{
    char **clean_args = malloc(sizeof(char *) * (arg_count + 1));
    if (clean_args == NULL)
        _exit(1);
    int clean_count = 0;
    for (int i = 0; i < arg_count; i++)
    {
        if (strcmp(args[i], "<") == 0 ||
            strcmp(args[i], ">") == 0 ||
            strcmp(args[i], ">>") == 0)
        {
            i++;
            continue;
        }
        clean_args[clean_count++] = args[i];
    }
    clean_args[clean_count] = NULL;
    int has_input = 0;
    for (int i = 0; i < arg_count; i++) {
        if (strcmp(args[i], "<") == 0) { has_input = 1; break; }
    }
    if (!has_input) {
        int fd = open("/dev/null", O_RDONLY);
        if (fd >= 0) {
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
    }
    int saved_stdout = dup(STDOUT_FILENO);
    if (saved_stdout < 0)
    {
        free(clean_args);
        _exit(1);
    }
    int input_status =
        setup_input_redirection(args, arg_count);
    if (input_status == -1)
    {
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
        free(clean_args);
        _exit(1);
    }
    FILE *output_tmp = NULL;
    int output_status =
        setup_output_redirection(args,
                                 arg_count,
                                 &output_tmp);
    if (output_status == -1)
    {
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
        free(clean_args);
        _exit(1);
    }
    if (clean_count == 0)
    {
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
        if (output_tmp != NULL)
            fclose(output_tmp);
        free(clean_args);
        _exit(1);
    }
    if (strcmp(clean_args[0], "hop") == 0)
    {
        execute_hop(clean_args,
                    clean_count,
                    shell_home,
                    prev_dir);
    }
    else if (strcmp(clean_args[0], "reveal") == 0)
    {
        execute_reveal(clean_args,
                       clean_count,
                       shell_home,
                       prev_dir);
    }
    else if (strcmp(clean_args[0], "peek") == 0)
    {
        peek(clean_count, clean_args);
    }
    else if (strcmp(clean_args[0], "locate") == 0)
    {
        locate(clean_count, clean_args);
    }    else if (strcmp(clean_args[0], "activities") == 0)
    {
        extern void print_activities(void);
        print_activities();
    }
    else if (strcmp(clean_args[0], "resume") == 0)
    {
        // Ignore in pipe or call execute_resume (not expected in pipe)
    }
    else if (strcmp(clean_args[0], "ping") == 0)
    {
        // Ignore in pipe
    }
    else if (strcmp(clean_args[0], "spy") == 0)
    {
        execute_spy(clean_args, clean_count);
    }
    else if (strcmp(clean_args[0], "snoop") == 0)
    {
        execute_snoop(clean_args, clean_count, shell_home, prev_dir);
    }
    else if (strcmp(clean_args[0], "resume") == 0)
    {
        // Ignore in pipe or call execute_resume (not expected in pipe)
    }
    else if (strcmp(clean_args[0], "ping") == 0)
    {
        // Ignore in pipe
    }
    else if (strcmp(clean_args[0], "spy") == 0)
    {
        execute_spy(clean_args, clean_count);
    }
    else if (strcmp(clean_args[0], "snoop") == 0)
    {
        execute_snoop(clean_args, clean_count, shell_home, prev_dir);
    }
    else
    {
        int is_ext = 0;
        for (int i = 0; i < clean_count; i++) {}
        if (check_external_exists(clean_args, clean_count)) is_ext = 1;
        if (is_ext) {
            char *path = getenv("PATH");
            int skip_cwd = 0;
            char *cmd = clean_args[0];
            if (cmd[0] == '%') { cmd = cmd + 1; skip_cwd = 1; }
            if (strchr(cmd, '/') != NULL) {
                execv(cmd, clean_args);
                _exit(1);
            }
            if (!skip_cwd) {
                char local_path[4096];
                snprintf(local_path, sizeof(local_path), "./%s", cmd);
                if (is_regular_executable(local_path)) {
                    execv(local_path, clean_args);
                    _exit(1);
                }
            }
            if (path != NULL) {
                char *pc = strdup(path);
                if (pc != NULL) {
                    char *dir = strtok(pc, ":");
                    while (dir != NULL) {
                        char fp[4096];
                        snprintf(fp, sizeof(fp), "%s/%s", dir, cmd);
                        if (is_regular_executable(fp)) {
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
    }
    if (dup2(saved_stdout, STDOUT_FILENO) < 0)
    {
        close(saved_stdout);
        free(clean_args);
        _exit(1);
    }
    close(saved_stdout);
    if (output_status == 1)
    {
        distribute_output(args,
                          arg_count,
                          output_tmp);
    }
    free(clean_args);
    _exit(0);
}
int execute_pipeline(char *raw_line,
                      char *shell_home,
                      char *prev_dir, pid_t *out_pids, char out_cmds[][256], int *stopped)
{
    char *line_copy = strdup(raw_line);
    if (line_copy == NULL)
        return 0;
    char *commands[MAX_PIPE_COMMANDS];
    int command_count = 0;
    char *command = strtok(line_copy, "|");
    while (command != NULL)
    {
        if (command_count >= MAX_PIPE_COMMANDS)
        {
            fprintf(stderr, "cshell: invalid syntax\n");
            free(line_copy);
            return 0;
        }
        commands[command_count] = command;
        if (out_cmds != NULL) {
            char *tmp = command;
            while(*tmp == ' ' || *tmp == '\t') tmp++;
            char cmd_name[256] = {0};
            int k=0;
            while(tmp[k] && tmp[k] != ' ' && tmp[k] != '\t' && k < 255) {
                cmd_name[k] = tmp[k];
                k++;
            }
            cmd_name[k] = '\0';
            strncpy(out_cmds[command_count], cmd_name, 255);
        }
        command_count++;
        command = strtok(NULL, "|");
    }
    if (command_count < 2)
    {
        fprintf(stderr, "cshell: invalid syntax\n");
        free(line_copy);
        return 0;
    }
    int pipes[MAX_PIPE_COMMANDS - 1][2];
    for (int i = 0; i < command_count - 1; i++)
    {
        if (pipe(pipes[i]) < 0)
        {
            perror("pipe");
            for (int j = 0; j < i; j++)
            {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            free(line_copy);
            return 0;
        }
    }
    pid_t pids[MAX_PIPE_COMMANDS];
    sigset_t mask, prev;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, &prev);
    for (int i = 0; i < command_count; i++)
    {
        pids[i] = fork();
        if (pids[i] > 0) {
            setpgid(pids[i], i == 0 ? pids[i] : pids[0]);
        }
        if (pids[i] < 0)
        {
            perror("fork");
            for (int j = 0; j < command_count - 1; j++)
            {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            for (int j = 0; j < i; j++)
                waitpid(pids[j], NULL, 0);
            sigprocmask(SIG_SETMASK, &prev, NULL);
            free(line_copy);
            return 0;
        }
        if (pids[i] == 0)
        {
            sigprocmask(SIG_SETMASK, &prev, NULL);
            setpgid(0, i == 0 ? 0 : pids[0]);
            if (i > 0)
            {
                if (dup2(pipes[i - 1][0], STDIN_FILENO) < 0)
                    _exit(1);
            }
            if (i < command_count - 1)
            {
                if (dup2(pipes[i][1], STDOUT_FILENO) < 0)
                    _exit(1);
            }
            for (int j = 0; j < command_count - 1; j++)
            {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            int ok = 1;
            token *token_head = lexer(commands[i], &ok);
            if (ok == 0 || token_head == NULL)
            {
                if (token_head)
                    free_tokens(token_head);
                _exit(1);
            }
            int grammar_check = parser(token_head);
            if (grammar_check == 1)
            {
                fprintf(stderr, "cshell: invalid syntax\n");
                free_tokens(token_head);
                _exit(1);
            }
            token *tmp = token_head;
            int arg_count = 0;
            while (tmp != NULL)
            {
                arg_count++;
                tmp = tmp->next_token;
            }
            char **args =
                malloc(sizeof(char *) * (arg_count + 1));
            if (args == NULL)
            {
                free_tokens(token_head);
                _exit(1);
            }
            tmp = token_head;
            for (int j = 0; j < arg_count; j++)
            {
                args[j] = tmp->content;
                tmp = tmp->next_token;
            }
            args[arg_count] = NULL;
            execute_command(args,
                            arg_count,
                            shell_home,
                            prev_dir);
            free(args);
            free_tokens(token_head);
            _exit(0);
        }
    }
    for (int i = 0; i < command_count - 1; i++)
    {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    tcsetpgrp(STDIN_FILENO, pids[0]);
    int is_stopped = 0;
    for (int i = 0; i < command_count; i++)
    {
        int status;
        waitpid(pids[i], &status, WUNTRACED);
        if (WIFSTOPPED(status)) is_stopped = 1;
        if (out_pids) out_pids[i] = pids[i];
    }
    tcsetpgrp(STDIN_FILENO, getpgrp());
    sigprocmask(SIG_SETMASK, &prev, NULL);
    if (stopped) *stopped = is_stopped;
    free(line_copy);
    return command_count;
}
int execute_pipeline_bg(char *raw_line,
                      char *shell_home,
                      char *prev_dir, pid_t *out_pids, char out_cmds[][256])
{
    char *line_copy = strdup(raw_line);
    if (line_copy == NULL)
        return -1;
    char *commands[MAX_PIPE_COMMANDS];
    int command_count = 0;
    char *command = strtok(line_copy, "|");
    while (command != NULL)
    {
        if (command_count >= MAX_PIPE_COMMANDS)
        {
            fprintf(stderr, "cshell: invalid syntax\n");
            free(line_copy);
            return -1;
        }
        commands[command_count] = command;
        if (out_cmds != NULL) {
            // strip leading spaces for cmd name
            char *tmp = command;
            while(*tmp == ' ' || *tmp == '	') tmp++;
            // get the first word
            char cmd_name[256] = {0};
            int k=0;
            while(tmp[k] && tmp[k] != ' ' && tmp[k] != '	' && k < 255) {
                cmd_name[k] = tmp[k];
                k++;
            }
            cmd_name[k] = '\0';
            strncpy(out_cmds[command_count], cmd_name, 255);
        }
        command_count++;
        command = strtok(NULL, "|");
    }
    if (command_count < 2)
    {
        fprintf(stderr, "cshell: invalid syntax\n");
        free(line_copy);
        return -1;
    }
    int pipes[MAX_PIPE_COMMANDS - 1][2];
    for (int i = 0; i < command_count - 1; i++)
    {
        if (pipe(pipes[i]) < 0)
        {
            perror("pipe");
            for (int j = 0; j < i; j++)
            {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            free(line_copy);
            return -1;
        }
    }
    pid_t pids[MAX_PIPE_COMMANDS];
    for (int i = 0; i < command_count; i++)
    {
        pids[i] = fork();
        if (pids[i] > 0) {
            setpgid(pids[i], i == 0 ? pids[i] : pids[0]);
        }
        if (pids[i] < 0)
        {
            perror("fork");
            for (int j = 0; j < command_count - 1; j++)
            {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            for (int j = 0; j < i; j++)
                kill(pids[j], SIGTERM);
            free(line_copy);
            return -1;
        }
        if (pids[i] == 0)
        {
            setpgid(0, i == 0 ? 0 : pids[0]);
            if (i > 0)
            {
                if (dup2(pipes[i - 1][0], STDIN_FILENO) < 0)
                    _exit(1);
            }
            if (i < command_count - 1)
            {
                if (dup2(pipes[i][1], STDOUT_FILENO) < 0)
                    _exit(1);
            }
            for (int j = 0; j < command_count - 1; j++)
            {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            int ok = 1;
            token *token_head = lexer(commands[i], &ok);
            if (ok == 0 || token_head == NULL)
            {
                if (token_head)
                    free_tokens(token_head);
                _exit(1);
            }
            int grammar_check = parser(token_head);
            if (grammar_check == 1)
            {
                fprintf(stderr, "cshell: invalid syntax\n");
                free_tokens(token_head);
                _exit(1);
            }
            token *tmp = token_head;
            int arg_count = 0;
            while (tmp != NULL)
            {
                arg_count++;
                tmp = tmp->next_token;
            }
            char **args =
                malloc(sizeof(char *) * (arg_count + 1));
            if (args == NULL)
            {
                free_tokens(token_head);
                _exit(1);
            }
            tmp = token_head;
            for (int j = 0; j < arg_count; j++)
            {
                args[j] = tmp->content;
                tmp = tmp->next_token;
            }
            args[arg_count] = NULL;
            execute_command(args,
                            arg_count,
                            shell_home,
                            prev_dir);
            free(args);
            free_tokens(token_head);
            _exit(0);
        }
    }
    for (int i = 0; i < command_count - 1; i++)
    {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    for (int i=0; i<command_count; i++) {
        out_pids[i] = pids[i];
    }
    free(line_copy);
    return command_count;
}
