#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

int check_external_exists(char **args, int arg_count)
{
    if (arg_count == 0 || args[0] == NULL)
        return 0;
    if (strchr(args[0], '/') != NULL) {
        if (access(args[0], X_OK) == 0)
            return 1;
        return 0;
    }
    char *cmd = args[0];
    char *lookup = cmd;
    if (cmd[0] == '%')
        lookup = cmd + 1;
    if (lookup[0] == '\0')
        return 0;
    char local_path[4096];
    snprintf(local_path, sizeof(local_path), "./%s", lookup);
    if (access(local_path, X_OK) == 0)
        return 1;
    char *path = getenv("PATH");
    if (path == NULL)
        return 0;
    char *path_copy = strdup(path);
    if (path_copy == NULL)
        return 0;
    int found = 0;
    char *dir = strtok(path_copy, ":");
    while (dir != NULL) {
        char found_path[4096];
        snprintf(found_path, sizeof(found_path), "%s/%s", dir, lookup);
        if (access(found_path, X_OK) == 0) {
            found = 1;
            break;
        }
        dir = strtok(NULL, ":");
    }
    free(path_copy);
    return found;
}
static int check_access_exists(char **args, int arg_count)
{
    return check_external_exists(args, arg_count);
}

int execute_external(char **args, int arg_count, pid_t *out_pid, int *stopped)
{
    if (arg_count == 0 || args[0] == NULL)
        return 0;
    if (check_access_exists(args, arg_count) == 0) {
        char *name = args[0];
        if (name[0] == '%')
            name = name + 1;
        fprintf(stderr, "cshell: command not found (%s)\n", name);
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
        return 0;
    }
    if (pid > 0) {
        // give terminal control to the child process group
        tcsetpgrp(STDIN_FILENO, pid);
        int status;
        waitpid(pid, &status, WUNTRACED);
        // reclaim terminal control
        tcsetpgrp(STDIN_FILENO, getpgrp());
        sigprocmask(SIG_SETMASK, &prev, NULL);
        if (WIFSTOPPED(status)) {
            if (stopped) *stopped = 1;
        }
        if (out_pid) *out_pid = pid;
        return 0;
    }
    setpgid(0, 0);
    sigprocmask(SIG_SETMASK, &prev, NULL);
    if (strchr(args[0], '/') != NULL) {
        execv(args[0], args);
        perror("execv");
        _exit(1);
    }
    char *cmd_name = args[0];
    if (args[0][0] == '%') {
        cmd_name = args[0] + 1;
        args[0] = cmd_name;
        char *path = getenv("PATH");
        if (path != NULL) {
            char *path_copy = strdup(path);
            if (path_copy != NULL) {
                char *dir = strtok(path_copy, ":");
                while (dir != NULL) {
                    char found_path[4096];
                    snprintf(found_path, sizeof(found_path), "%s/%s", dir, cmd_name);
                    if (access(found_path, X_OK) == 0) {
                        execv(found_path, args);
                        perror("execv");
                        _exit(1);
                    }
                    dir = strtok(NULL, ":");
                }
                free(path_copy);
            }
        }
        _exit(1);
    }
    char local_path[4096];
    snprintf(local_path, sizeof(local_path), "./%s", args[0]);
    if (access(local_path, X_OK) == 0) {
        execv(local_path, args);
        perror("execv");
        _exit(1);
    }
    char *path = getenv("PATH");
    if (path != NULL) {
        char *path_copy = strdup(path);
        if (path_copy != NULL) {
            char *dir = strtok(path_copy, ":");
            while (dir != NULL) {
                char found_path[4096];
                snprintf(found_path, sizeof(found_path), "%s/%s", dir, args[0]);
                if (access(found_path, X_OK) == 0) {
                    execv(found_path, args);
                    perror("execv");
                    _exit(1);
                }
                dir = strtok(NULL, ":");
            }
            free(path_copy);
        }
    }
    _exit(1);
}
