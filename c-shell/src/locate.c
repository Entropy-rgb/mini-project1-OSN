#include "locate.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>

int locate(int argc, char *argv[])
{
    if(argc < 2)
    {
        fprintf(stderr, "locate: invalid syntax\n");
        return 1;
    }

    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        cwd[0] = '\0';
    }

    for(int arg = 1; arg < argc ; arg++){
        bool found = false;
        char buffer[8192];
        
        if (cwd[0] != '\0') {
            snprintf(buffer, sizeof(buffer), "%s/%s", cwd, argv[arg]);
            if(access(buffer, X_OK) == 0){
                printf("%s\n", buffer);
                found = true;
            }
        }

        char *path_env_orig = getenv("PATH");
        if (path_env_orig != NULL) {
            char *path_env = strdup(path_env_orig);
            if (path_env != NULL) {
                char *dir = strtok(path_env, ":");
                while(dir != NULL){
                    snprintf(buffer, sizeof(buffer), "%s/%s", dir, argv[arg]);
                    if(access(buffer, X_OK) == 0){
                        printf("%s\n", buffer);
                        found = true;
                    }
                    dir = strtok(NULL, ":");
                }
                free(path_env);
            }
        }

        if(!found){
            fprintf(stderr, "locate: command not found (%s)\n", argv[arg]);
        }
    }
    return 0;
}