#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "lexer.h"
#include "parser.h"
#include "hop.h"
#include "reveal.h"
#include "peek.h"
#include "locate.h"
#include "exec_calls.h"
#include "redirection.h"
#include "pipe.h"

#define MAX_PIPE_COMMANDS 256

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
    }
    else
    {
        execute_external(clean_args, clean_count);
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

void execute_pipeline(char *raw_line,
                      char *shell_home,
                      char *prev_dir)
{
    char *line_copy = strdup(raw_line);

    if (line_copy == NULL)
        return;

    char *commands[MAX_PIPE_COMMANDS];
    int command_count = 0;

    char *command = strtok(line_copy, "|");

    while (command != NULL)
    {
        if (command_count >= MAX_PIPE_COMMANDS)
        {
            fprintf(stderr, "cshell: invalid syntax\n");
            free(line_copy);
            return;
        }

        commands[command_count++] = command;
        command = strtok(NULL, "|");
    }

    if (command_count < 2)
    {
        fprintf(stderr, "cshell: invalid syntax\n");
        free(line_copy);
        return;
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
            return;
        }
    }

    pid_t pids[MAX_PIPE_COMMANDS];

    for (int i = 0; i < command_count; i++)
    {
        pids[i] = fork();

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

            free(line_copy);
            return;
        }

        if (pids[i] == 0)
        {
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

    for (int i = 0; i < command_count; i++)
    {
        waitpid(pids[i], NULL, 0);
    }

    free(line_copy);
}