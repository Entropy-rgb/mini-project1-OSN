#include "peek.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>

int peek(int argc, char *argv[]) {
    bool number_lines = false;
    bool reverse_lines = false;
    char *filenames[1024];
    int file_count = 0;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            for (int j = 1; argv[i][j] != '\0'; j++) {
                if (argv[i][j] == 'n') {
                    number_lines = true;
                } else if (argv[i][j] == 'r') {
                    reverse_lines = true;
                } else {
                    fprintf(stderr, "peek: invalid syntax\n");
                }
            }
        } else {
            if (file_count < 1024) {
                filenames[file_count++] = argv[i];
            } else {
                fprintf(stderr, "peek: too many arguments\n");
                return 1;
            }
        }
    }

    if (file_count == 0) {
        filenames[0] = "-";
        file_count = 1;
    }

    for (int i = 0; i < file_count; i++) {
        char *current_file = filenames[i];

        if (strcmp(current_file, "-") != 0) {
            struct stat path_stat;
            
            if (stat(current_file, &path_stat) != 0) {
                fprintf(stderr, "peek: no such file or directory\n");
                continue; // Skip to the next file
            }

            if (S_ISDIR(path_stat.st_mode)) {
                fprintf(stderr, "peek: is a directory\n");
                continue; // Skip to the next file
            }
        }

        // TODO: Step 3, 4, and 5 (File Reading Logic) go here
        // We now know current_file is either "-" or a valid, readable regular file.
    }

    return 0;
}