#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <pwd.h>
#include <sys/types.h>
#include <limits.h>
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
    if (strcmp(clean_args[0], "hop") == 0) {
        execute_hop(clean_args, clean_count, shell_home, prev_dir);
    } else if (strcmp(clean_args[0], "reveal") == 0) {
        execute_reveal(clean_args, clean_count, shell_home, prev_dir);
    } else if (strcmp(clean_args[0], "peek") == 0) {
        peek(clean_count, clean_args);
    } else if (strcmp(clean_args[0], "locate") == 0) {
        locate(clean_count, clean_args);
    } else {
        ret = execute_external(clean_args, clean_count);
    }
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
        ssize_t nread = getline(&line, &len, stdin);
        if (nread == -1) {
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
        while (true) {
            int is_end = (cur == NULL);
            int is_semi = (!is_end && cur->type == OP_SEMI);
            if (is_end || is_semi) {
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
                            execute_pipeline(pipe_line, shell_home, prev_dir);
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
                            int st = execute_single(args, count, shell_home, prev_dir);
                            free(args);
                            if (st == 1) {
                                break;
                            }
                        }
                    }
                }
                if (is_end)
                    break;
                seg_start = cur->next_token;
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
