#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "hop.h"

void update_frecency(char *shell_home, const char *new_path)
{
    FrecencyEntry *saved_entries = malloc(sizeof(FrecencyEntry) * 64);
    if (saved_entries == NULL)
        return;

    char frecency_file_location[4096];
    snprintf(frecency_file_location, sizeof(frecency_file_location), "%s/.hop_history", shell_home);

    int count = 0;

    FILE *fptr = fopen(frecency_file_location, "r");
    if (fptr != NULL)
    {
        while (count < 64 && fscanf(fptr, "%d %ld %[^\n]", &saved_entries[count].frequency, &saved_entries[count].recency, saved_entries[count].path) == 3)
        {
            count++;
        }
        fclose(fptr);
    }

    int found = 0;
    for (int i = 0; i < count; i++)
    {
        if (strcmp(saved_entries[i].path, new_path) == 0)
        {
            saved_entries[i].frequency += 1;
            saved_entries[i].recency = (long)time(NULL);
            found = 1;
            break;
        }
    }

    if (!found && count < 64)
    {
        strcpy(saved_entries[count].path, new_path);
        saved_entries[count].frequency = 1;
        saved_entries[count].recency = (long)time(NULL);
        count++;
    }

    fptr = fopen(frecency_file_location, "w");
    if (fptr != NULL)
    {
        for (int i = 0; i < count; i++)
        {
            fprintf(fptr, "%d %ld %s\n", saved_entries[i].frequency, saved_entries[i].recency, saved_entries[i].path);
        }
        fclose(fptr);
    }

    free(saved_entries);
}

char *search_frecency(char *shell_home, const char *target)
{
    char frecency_file_location[4096];
    snprintf(frecency_file_location, sizeof(frecency_file_location), "%s/.hop_history", shell_home);

    FILE *fptr = fopen(frecency_file_location, "r");
    if (fptr == NULL)
        return NULL;

    char best_match[4096] = "";
    long long best_score = -1;

    char path[4096];
    int freq;
    long rec;

    while (fscanf(fptr, "%d %ld %[^\n]", &freq, &rec, path) == 3)
    {
        if (strstr(path, target) != NULL)
        {
            if (access(path, F_OK) == 0)
            {
                long long score = ((long long)freq * 10000000LL) + rec;
                if (score > best_score)
                {
                    best_score = score;
                    strcpy(best_match, path);
                }
            }
        }
    }
    fclose(fptr);

    if (best_score != -1)
    {
        char *winner = malloc(strlen(best_match) + 1);
        strcpy(winner, best_match);
        return winner;
    }

    return NULL;
}

int execute_hop(char **args, int arg_count, char *shell_home, char *prev_dir)
{
    char owd[4096];
    char new_cwd[4096];
    int failure = 1;

    if (arg_count == 1)
    {
        getcwd(owd, 4096);
        failure = chdir(shell_home);
        if (!failure)
        {
            strcpy(prev_dir, owd);
            getcwd(new_cwd, 4096);
            update_frecency(shell_home, new_cwd);
        }
        else
        {
            fprintf(stderr, "hop: no such directory\n");
        }
        return 0;
    }

    if (arg_count > 1)
    {
        for (int each_arg = 1; each_arg < arg_count; each_arg++)
        {
            getcwd(owd, 4096);
            if (strcmp(args[each_arg], "~") == 0)
            {
                failure = chdir(shell_home);
            }
            else if (strcmp(args[each_arg], "-") == 0)
            {
                if (*prev_dir)
                {
                    failure = chdir(prev_dir);
                }
                else
                {
                    continue;
                }
            }
            else if (strcmp(args[each_arg], ".") == 0)
            {
                continue;
            }
            else
            {
                failure = chdir(args[each_arg]);
                if (failure)
                {
                    char *fallback_path = search_frecency(shell_home, args[each_arg]);
                    if (fallback_path != NULL)
                    {
                        failure = chdir(fallback_path);
                        free(fallback_path);
                    }
                }
            }
            if (!failure)
            {
                strcpy(prev_dir, owd);
                getcwd(new_cwd, 4096);
                update_frecency(shell_home, new_cwd);
            }
            else
            {
                fprintf(stderr, "hop: no such directory\n");
            }
        }
    }
    return 0;
}