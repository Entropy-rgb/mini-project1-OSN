#ifndef LEXER_H
#define LEXER_H

#include "token.h"

token *lexer(char *line, int *ok);
void free_tokens(token *token_head);
#endif