#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

void execute_external(char **args, int arg_count)
{
    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork");
        return;
    }

    if (pid > 0)
    {
        wait(NULL);
        return;
    }

    if (strchr(args[0], '/') != NULL)
    {
        if (access(args[0], X_OK) == 0)
        {
            execv(args[0], args);
        }

        fprintf(stderr, "cshell: command not found (%s)\n", args[0]);
        exit(1);
    }

    char *cmd_name = args[0];

    if (args[0][0] == '%')
    {
        cmd_name = args[0] + 1;

        char *path = getenv("PATH");
        if (path != NULL)
        {
            char *path_copy = strdup(path);
            if (path_copy != NULL)
            {
                char *dir = strtok(path_copy, ":");

                while (dir != NULL)
                {
                    char found_path[4096];

                    snprintf(found_path, sizeof(found_path),
                             "%s/%s", dir, cmd_name);

                    if (access(found_path, X_OK) == 0)
                    {
                        execv(found_path, args);
                    }

                    dir = strtok(NULL, ":");
                }

                free(path_copy);
            }
        }

        fprintf(stderr, "cshell: command not found (%s)\n", cmd_name);
        exit(1);
    }

    char local_path[4096];

    snprintf(local_path, sizeof(local_path), "./%s", args[0]);

    if (access(local_path, X_OK) == 0)
    {
        execv(local_path, args);
    }

    char *path = getenv("PATH");

    if (path != NULL)
    {
        char *path_copy = strdup(path);

        if (path_copy != NULL)
        {
            char *dir = strtok(path_copy, ":");

            while (dir != NULL)
            {
                char found_path[4096];

                snprintf(found_path, sizeof(found_path),
                         "%s/%s", dir, args[0]);

                if (access(found_path, X_OK) == 0)
                {
                    execv(found_path, args);
                }

                dir = strtok(NULL, ":");
            }

            free(path_copy);
        }
    }

    fprintf(stderr, "cshell: command not found (%s)\n", args[0]);
    exit(1);
}