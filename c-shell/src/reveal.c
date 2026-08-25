#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>

int cmp_func(const void *a, const void *b)
{
    return strcmp(*(const char **)a, *(const char **)b);
}

void reveal_dir(const char *path, const char *prefix, bool show_hidden, bool is_recursive)
{
    DIR *dptr = opendir(path);
    if (dptr == NULL)
    {
        fprintf(stderr, "reveal: no such directory\n");
        return;
    }

    char **files = malloc(4096 * sizeof(char *));
    int entry_count = 0;
    struct dirent *entry;

    while ((entry = readdir(dptr)) != NULL)
    {
        files[entry_count] = strdup(entry->d_name);
        entry_count++;
    }
    closedir(dptr);

    qsort(files, entry_count, sizeof(char *), cmp_func);

    for (int i = 0; i < entry_count; i++)
    {
        if (!show_hidden && files[i][0] == '.')
        {
            continue;
        }

        char new_path[4096];
        snprintf(new_path, sizeof(new_path), "%s/%s", path, files[i]);

        struct stat stat_struct;
        bool is_dir = false;

        if (stat(new_path, &stat_struct) == 0 && S_ISDIR(stat_struct.st_mode))
        {
            is_dir = true;
        }

        if (is_dir)
        {
            printf("%s%s/\n", prefix, files[i]);
        }
        else
        {
            printf("%s%s\n", prefix, files[i]);
        }

        if (is_recursive && is_dir)
        {
            if (strcmp(files[i], ".") != 0 && strcmp(files[i], "..") != 0)
            {
                char new_prefix[4096];
                snprintf(new_prefix, sizeof(new_prefix), "%s%s/", prefix, files[i]);

                reveal_dir(new_path, new_prefix, show_hidden, is_recursive);
            }
        }
    }

    for (int i = 0; i < entry_count; i++)
    {
        free(files[i]);
    }
    free(files);
}

void execute_reveal(char **args, int arg_count, char *shell_home, char *prev_dir)
{
    bool show_hidden = false;
    bool is_recursive = false;
    char *target_dir = malloc(sizeof(char) * 4096);
    strcpy(target_dir, ".");

    for (int arg = 1; arg < arg_count; arg++)
    {
        if (args[arg][0] == '-' && strlen(args[arg]) > 1)
        {
            for (size_t i = 1; i < strlen(args[arg]); i++)
            {
                if (args[arg][i] == 'a')
                {
                    show_hidden = true;
                }
                else if (args[arg][i] == 't')
                {
                    is_recursive = true;
                }
                else
                {
                    fprintf(stderr, "reveal: invalid syntax\n");
                    free(target_dir);
                    return;
                }
            }
        }
        else
        {
            if (strcmp(target_dir, ".") == 0)
            {
                strcpy(target_dir, args[arg]);
            }
            else
            {
                fprintf(stderr, "reveal: invalid syntax\n");
                free(target_dir);
                return;
            }
        }
    }

    if (strcmp(target_dir, "~") == 0)
    {
        strcpy(target_dir, shell_home);
    }
    else if (strcmp(target_dir, "-") == 0)
    {
        if (*prev_dir)
        {
            strcpy(target_dir, prev_dir);
        }
        else
        {
            fprintf(stderr, "reveal: no such directory\n");
            free(target_dir);
            return;
        }
    }

    reveal_dir(target_dir, "", show_hidden, is_recursive);
    free(target_dir);
}