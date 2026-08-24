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

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 256
#endif

int main()
{
    // get username using the getpwuid syscall
    uid_t user_uid = getuid();
    struct passwd *pw = getpwuid(user_uid);

    if(pw == NULL){
        fprintf(stderr, "shell : could not resolve username\n");
    }

    // get hostname using gethostname()
    char hostname[HOST_NAME_MAX];
    gethostname(hostname, HOST_NAME_MAX);

    // get the shell cwd , which is the shell home according to the requirement doc
    char shell_home[4096];
    getcwd(shell_home, sizeof(shell_home));
    int home_len = strlen(shell_home);

    char* line = NULL;
    size_t len = 0;

    while (true)
    {
        char cwd[4096];
        getcwd(cwd, sizeof(cwd));
        char rel_working_dir[4096] = "~";
        if (strncmp(cwd, shell_home, home_len) == 0 && (cwd[home_len] == '\0' || cwd[home_len] == '/'))
        {
            strcat(rel_working_dir, &cwd[home_len]);
        }
        else
        {
            strcpy(rel_working_dir, cwd);
        }
        printf("<%s@%s:%s> ", pw->pw_name, hostname, rel_working_dir);
        ssize_t nread = getline(&line, &len, stdin);
        if(nread == -1){
            break;
        }
        line[strcspn(line, "\n")] = '\0';
        // send the input to lexer, get the lexer's output and send it to parser , and execute given commands
        int ok  = 1;
        token* token_head = lexer(line, &ok);

        if(ok == 0 || token_head == NULL){
            if(token_head) free_tokens(token_head);
            continue;
        }

        int grammar_check = parser(token_head);
        if(grammar_check == 1){
            fprintf(stderr, "cshell: invalid syntax\n");
            continue;
        }

    }
    free(line);
    return 0;
}