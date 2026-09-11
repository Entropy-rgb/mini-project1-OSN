#ifndef EXEC_CALLS_H
#define EXEC_CALLS_H

int execute_external(char **args, int arg_count, pid_t *out_pid, int *stopped);
int check_external_exists(char **args, int arg_count);

#endif
char* check_external_command(char *command);
