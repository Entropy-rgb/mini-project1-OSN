#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "prompt.h"

void display_prompt(const char *username, const char *hostname, const char *shell_home)
{
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("shell: getcwd failed");
        return;
    }

    int home_len = strlen(shell_home);
    char rel_working_dir[4096] = "~";

    // If cwd starts with shell_home and is correctly bounded by null or a slash
    if (strncmp(cwd, shell_home, home_len) == 0 && (cwd[home_len] == '\0' || cwd[home_len] == '/'))
    {
        strcat(rel_working_dir, &cwd[home_len]);
    }
    else
    {
        strcpy(rel_working_dir, cwd);
    }

    printf("<%s@%s:%s> ", username, hostname, rel_working_dir);
    
    // Force the prompt to print immediately since there is no newline character
    fflush(stdout); 
}