#include "peek.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#define CHUNK_SIZE 4096

static bool is_not_empty(const char *line)
{
    if (!line) return false;
    while (*line != '\0') {
        if (*line != ' ' && *line != '\t' && *line != '\n' && *line != '\r') return true;
        line++;
    }
    return false;
}

int peek(int argc, char *argv[])
{
    bool number_lines = false;
    bool reverse_lines = false;
    char *filenames[1024];
    int file_count = 0;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            for (int j = 1; argv[i][j] != '\0'; j++) {
                if (argv[i][j] == 'n') number_lines = true;
                else if (argv[i][j] == 'r') reverse_lines = true;
                else {
                    fprintf(stderr, "peek: invalid syntax\n");
                    return 1;
                }
            }
        } else {
            if (file_count < 1024) filenames[file_count++] = argv[i];
            else {
                fprintf(stderr, "peek: too many arguments\n");
                return 1;
            }
        }
    }

    if (file_count == 0) {
        filenames[0] = "-";
        file_count = 1;
    }

    int global_line_num = 1;

    for (int i = 0; i < file_count; i++) {
        char *current_file = filenames[i];

        if (strcmp(current_file, "-") != 0) {
            struct stat path_stat;
            if (stat(current_file, &path_stat) != 0) {
                fprintf(stderr, "peek: no such file or directory\n");
                continue;
            }
            if (S_ISDIR(path_stat.st_mode)) {
                fprintf(stderr, "peek: is a directory\n");
                continue;
            }
        }

        if (!reverse_lines) {
            FILE *fp = (strcmp(current_file, "-") == 0) ? stdin : fopen(current_file, "r");
            if (!fp) {
                perror("peek");
                continue;
            }

            char *line = NULL;
            size_t len = 0;

            while (getline(&line, &len, fp) != -1) {
                if (number_lines && is_not_empty(line)) {
                    printf("%d %s", global_line_num++, line);
                } else {
                    printf("%s", line);
                }
                if (line[strlen(line) - 1] != '\n') printf("\n");
            }
            free(line);
            if (fp != stdin) fclose(fp);
        }
        else if (strcmp(current_file, "-") == 0) {
            int cap = 1024;
            char **lines = malloc(cap * sizeof(char *));
            int count = 0;
            int total_non_empty = 0;

            char *line = NULL;
            size_t len = 0;

            while (getline(&line, &len, stdin) != -1) {
                if (count >= cap) {
                    cap *= 2;
                    lines = realloc(lines, cap * sizeof(char *));
                }
                lines[count++] = strdup(line);
                if (is_not_empty(line)) total_non_empty++;
            }
            free(line);

            int current_line_num = global_line_num + total_non_empty - 1;
            global_line_num += total_non_empty;

            for (int j = count - 1; j >= 0; j--) {
                if (number_lines && is_not_empty(lines[j])) {
                    printf("%d %s", current_line_num--, lines[j]);
                } else {
                    printf("%s", lines[j]);
                }
                if (lines[j][strlen(lines[j]) - 1] != '\n') printf("\n");
                free(lines[j]);
            }
            free(lines);
        }
        else {
            int total_non_empty = 0;
            FILE *fp = fopen(current_file, "r");
            if (fp) {
                char *line = NULL;
                size_t len = 0;
                while (getline(&line, &len, fp) != -1) {
                    if (is_not_empty(line)) total_non_empty++;
                }
                free(line);
                fclose(fp);
            }

            int current_line_num = global_line_num + total_non_empty - 1;
            global_line_num += total_non_empty;

            int fd = open(current_file, O_RDONLY);
            if (fd == -1) {
                perror("peek");
                continue;
            }

            off_t file_size = lseek(fd, 0, SEEK_END);
            off_t pos = file_size;
            char chunk[CHUNK_SIZE];
            char *leftover = NULL;
            size_t leftover_len = 0;

            while (pos > 0) {
                size_t read_size = (pos < CHUNK_SIZE) ? pos : CHUNK_SIZE;
                pos -= read_size;
                lseek(fd, pos, SEEK_SET);

                if (read(fd, chunk, read_size) != (ssize_t)read_size) break;

                int segment_end = read_size;
                for (int j = read_size - 1; j >= 0; j--) {
                    if (chunk[j] == '\n') {
                        int seg_len = segment_end - (j + 1);
                        size_t full_len = seg_len + leftover_len;

                        char *full_line = malloc(full_len + 2);
                        if (seg_len > 0) memcpy(full_line, chunk + j + 1, seg_len);
                        if (leftover_len > 0) memcpy(full_line + seg_len, leftover, leftover_len);
                        full_line[full_len] = '\n';
                        full_line[full_len+1] = '\0';

                        if (number_lines && is_not_empty(full_line)) {
                            printf("%d %s", current_line_num--, full_line);
                        } else {
                            printf("%s", full_line);
                        }
                        free(full_line);

                        segment_end = j;
                        if (leftover) {
                            free(leftover);
                            leftover = NULL;
                            leftover_len = 0;
                        }
                    }
                }

                if (segment_end > 0) {
                    char *new_left = malloc(segment_end + leftover_len);
                    memcpy(new_left, chunk, segment_end);
                    if (leftover_len > 0) memcpy(new_left + segment_end, leftover, leftover_len);
                    if (leftover) free(leftover);
                    leftover = new_left;
                    leftover_len = segment_end + leftover_len;
                }
            }

            if (leftover_len > 0) {
                char *full_line = malloc(leftover_len + 2);
                memcpy(full_line, leftover, leftover_len);
                // The first line in the file doesn't have a preceding newline
                // But it might not have a trailing one. Add one so it prints nicely if it's missing.
                full_line[leftover_len] = '\n';
                full_line[leftover_len+1] = '\0';

                if (number_lines && is_not_empty(full_line)) {
                    printf("%d %s", current_line_num--, full_line);
                } else {
                    printf("%s", full_line);
                }
                free(full_line);
                free(leftover);
            }
            close(fd);
        }
    }

    return 0;
}
