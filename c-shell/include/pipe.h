#ifndef PIPE_H
#define PIPE_H
#define MAX_PIPE_COMMANDS 256
#include <sys/types.h>
int execute_pipeline(char *raw_line, char *shell_home, char *prev_dir, pid_t *out_pids, char out_cmds[][256], int *stopped);
int execute_pipeline_bg(char *raw_line, char *shell_home, char *prev_dir, pid_t *out_pids, char out_cmds[][256]);

#endif