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

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 256
#endif

int main()
{
    // get username using the getpwuid syscall
    uid_t user_uid = getuid();
    struct passwd *pw = getpwuid(user_uid);

    const char *username = "unknown";
    if(pw == NULL){
        fprintf(stderr, "shell : could not resolve username\n");
    } else {
        username = pw->pw_name;
    }

    // get hostname using gethostname()
    char hostname[HOST_NAME_MAX];
    if (gethostname(hostname, HOST_NAME_MAX) != 0) {
        strcpy(hostname, "unknown");
    }

    // get the shell cwd , which is the shell home according to the requirement doc
    char shell_home[4096];
    getcwd(shell_home, sizeof(shell_home));    

    char* line = NULL;
    size_t len = 0;
    char prev_dir[4096] = "";
    while (true)
    {
        display_prompt(username, hostname, shell_home);
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
            free_tokens(token_head); // Clean up memory before continuing
            continue;
        }

        // making the array of strings , so that they will also be useful in future subparts of the assignment
        token* tmp = token_head;
        int arg_count = 0;
        while(tmp!=NULL){
            arg_count++;
            tmp = tmp->next_token;
        }

        char** args = malloc(sizeof(char *)*(arg_count+1));
        tmp = token_head;
        for(int i = 0; i < arg_count; i++){
            args[i] = tmp->content;
            tmp = tmp->next_token;
        }
        args[arg_count] = NULL;

        if(strcmp(args[0], "hop") == 0){
            execute_hop(args , arg_count, shell_home, prev_dir);
        }
        free_tokens(token_head);
        free(args);
    }
    free(line);
    return 0;
}