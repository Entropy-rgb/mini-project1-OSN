#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "hop.h"

int execute_hop(char **args , int arg_count , char *shell_home , char *prev_dir){

    char owd[4096];
    int failure = 1;

    if(arg_count == 1){
        getcwd(owd, 4096);

        failure = chdir(shell_home);
        if(!failure){
            strcpy(prev_dir , owd);
        }else{
            fprintf(stderr, "hop: no such directory\n");
        }

        return 0;
    }

    if(arg_count > 1){
        for(int each_arg = 1 ; each_arg < arg_count; each_arg++){
            getcwd(owd, 4096);
            if(strcmp(args[each_arg], "~") == 0){
                failure = chdir(shell_home);
                if(!failure){
                    strcpy(prev_dir , owd);
                }else{
                    fprintf(stderr, "hop: no such directory\n");
                }
            }else if(strcmp(args[each_arg], "-")==0){
                if(*prev_dir){
                    failure = chdir(prev_dir);
                    if(!failure){
                        strcpy(prev_dir , owd);
                    }else{
                        fprintf(stderr, "hop: no such directory\n");
                    }
                }else{
                    continue;
                }
            }else if(strcmp(args[each_arg], ".")==0){
                continue;
            }else{
                failure = chdir(args[each_arg]);
                if(!failure){
                    strcpy(prev_dir , owd);
                }else{
                    fprintf(stderr, "hop: no such directory\n");
                }
            }
        }
    }
    return 0;
}