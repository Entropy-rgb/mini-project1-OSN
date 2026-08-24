#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "token.h"

enum lexer_states
{
    START,
    IN_WORD,
    IN_DQUOTE,
    IN_SQUOTE,
    AFTER_ESCAPE,
};

// Updated to use double pointers to modify the head and tail directly
void append_token(token **head, token **tail, char *content, enum TokenType type)
{
    token *new_token = malloc(sizeof(token));
    new_token->content = content;
    new_token->type = type;
    new_token->next_token = NULL;
    
    if (*head == NULL) {
        // First token in the list
        *head = new_token;
        *tail = new_token;
    } else {
        // Append to the end
        (*tail)->next_token = new_token;
        *tail = new_token;
    }
}

token *lexer(char *line, int *ok)
{
    enum lexer_states curr_state = START;
    int curr_char = 0;
    char currently_reading;
    
    // Start with a truly empty list
    token *token_head = NULL;
    token *token_tail = NULL;

    *ok = 1; // Assume success initially

    char buffer[4096]; // Buffer to accumulate word characters
    int buf_idx = 0;

    while (1)
    {
        currently_reading = line[curr_char];
        
        if (curr_state == START)
        {
            if (currently_reading == '\0') {
                break;
            }
            if (currently_reading == ' ' || currently_reading == '\t' || currently_reading == '\n' || currently_reading == '\r')
            {
                curr_char++;
                continue;
            }

            // Operators (passing &token_head and &token_tail)
            if (currently_reading == '|') {
                append_token(&token_head, &token_tail, strdup("|"), OP_PIPE);
                curr_char++;
            } else if (currently_reading == '&') {
                append_token(&token_head, &token_tail, strdup("&"), OP_AMP);
                curr_char++;
            } else if (currently_reading == ';') {
                append_token(&token_head, &token_tail, strdup(";"), OP_SEMI);
                curr_char++;
            } else if (currently_reading == '<') {
                if (line[curr_char + 1] == '<') {
                    append_token(&token_head, &token_tail, strdup("<<"), OP_LTLT);
                    curr_char += 2;
                } else {
                    append_token(&token_head, &token_tail, strdup("<"), OP_LT);
                    curr_char++;
                }
            } else if (currently_reading == '>') {
                if (line[curr_char + 1] == '>') {
                    append_token(&token_head, &token_tail, strdup(">>"), OP_GTGT);
                    curr_char += 2;
                } else {
                    append_token(&token_head, &token_tail, strdup(">"), OP_GT);
                    curr_char++;
                }
            } else if (currently_reading == '"') {
                curr_state = IN_DQUOTE;
                curr_char++;
            } else if (currently_reading == '\'') {
                curr_state = IN_SQUOTE;
                curr_char++;
            } else if (currently_reading == '\\') {
                curr_state = AFTER_ESCAPE;
                curr_char++;
            } else {
                // Start of a word
                curr_state = IN_WORD;
                buffer[buf_idx++] = currently_reading;
                curr_char++;
            }
        }
        else if (curr_state == IN_WORD)
        {
            if (currently_reading == '\0' || currently_reading == ' ' || currently_reading == '\t' || currently_reading == '\n' ||
                currently_reading == '|' || currently_reading == '&' || currently_reading == ';' || 
                currently_reading == '<' || currently_reading == '>') 
            {
                buffer[buf_idx] = '\0';
                append_token(&token_head, &token_tail, strdup(buffer), WORD);
                buf_idx = 0; 
                curr_state = START;
            } 
            else if (currently_reading == '"') {
                curr_state = IN_DQUOTE;
                curr_char++;
            } else if (currently_reading == '\'') {
                curr_state = IN_SQUOTE;
                curr_char++;
            } else if (currently_reading == '\\') {
                curr_state = AFTER_ESCAPE;
                curr_char++;
            } else {
                buffer[buf_idx++] = currently_reading;
                curr_char++;
            }
        }
        else if (curr_state == IN_DQUOTE)
        {
            if (currently_reading == '\0') {
                fprintf(stderr, "Error: Unmatched double quote.\n");
                *ok = 0;
                break;
            } else if (currently_reading == '"') {
                curr_state = IN_WORD; 
            } else {
                buffer[buf_idx++] = currently_reading;
            }
            curr_char++;
        }
        else if (curr_state == IN_SQUOTE)
        {
            if (currently_reading == '\0') {
                fprintf(stderr, "Error: Unmatched single quote.\n");
                *ok = 0;
                break;
            } else if (currently_reading == '\'') {
                curr_state = IN_WORD;
            } else {
                buffer[buf_idx++] = currently_reading;
            }
            curr_char++;
        }
        else if (curr_state == AFTER_ESCAPE)
        {   
            if (currently_reading == '\0') {
                fprintf(stderr, "Error: Trailing escape character.\n");
                *ok = 0;
                break;
            }
            buffer[buf_idx++] = currently_reading;
            curr_state = IN_WORD;
            curr_char++;
        }
    }

    if (curr_state == IN_WORD && buf_idx > 0)
    {
        buffer[buf_idx] = '\0';
        append_token(&token_head, &token_tail, strdup(buffer), WORD);
    } 
    else if (curr_state != START && curr_state != IN_WORD) 
    {
        *ok = 0; 
    }

    return token_head;
}