#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <pwd.h>
#include <sys/types.h>
#include <limits.h>


int main(){
    //get username using the getpwuid syscall
    uid_t user_uid = getuid();
    struct passwd* pw = getpwuid(user_uid);

    //get hostname using gethostname()
    char hostname[_SC_HOST_NAME_MAX];
    gethostname(hostname, _SC_HOST_NAME_MAX);

    //get the shell cwd , which is the shell home according to the requirement doc
    char shell_home[4096];
    getcwd(shell_home, sizeof(shell_home));
    int home_len = strlen(shell_home);

    while(true){
        char cwd[4096];
        getcwd(cwd, sizeof(cwd));
        char rel_working_dir[4096] = "~";
        if(strncmp(cwd, shell_home, home_len) == 0 && (cwd[home_len] == '\0' || cwd[home_len] == '\\')){
            strcat(rel_working_dir, &cwd[home_len]);
        }else{
            strcpy(rel_working_dir, cwd);
        }
        printf("<%s@%s:%s>", pw->pw_name, hostname, rel_working_dir);
        char inpstr[50];
        scanf("%s", inpstr);
    }
    return 0;
}