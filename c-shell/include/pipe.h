#ifndef PIPE_H
#define PIPE_H
#include <sys/types.h>
void execute_pipeline(char *raw_line, char *shell_home, char *prev_dir);
pid_t execute_pipeline_bg(char *raw_line, char *shell_home, char *prev_dir);

#endif