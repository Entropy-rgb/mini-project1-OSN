#ifndef REDIRECTION_H
#define REDIRECTION_H

#include <stdio.h>

int setup_input_redirection(char **args, int arg_count);

int setup_output_redirection(char **args, int arg_count, FILE **out_tmp);

int distribute_output(char **args, int arg_count, FILE *out_tmp);

#endif