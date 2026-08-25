#ifndef HOP_H
#define HOP_H

#include "token.h"

typedef struct
{
    char path[4096];
    int frequency;
    long recency; // using time(NULL) for this is the easiest approach
} FrecencyEntry;

int execute_hop(char **args, int arg_count, char *shell_home, char *prev_dir);

#endif