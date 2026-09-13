#include <stdio.h>
#include <stdlib.h>
#include "token.h"

enum parser_states {
    q_start,
    q_arg,
    q_cmd,
    q_tgt,
    q_bg,
    q_error
};

int parser(token *head) {
    enum parser_states curr_state = q_start;
    while (head != NULL) {
        if (curr_state == q_start) {
            if (head->type == WORD) curr_state = q_arg;
            else { curr_state = q_error; break; }
        }
        else if (curr_state == q_arg) {
            if (head->type == WORD) curr_state = q_arg;
            else if (head->type == OP_LT || head->type == OP_GT || head->type == OP_GTGT || head->type == OP_LTLT) curr_state = q_tgt;
            else if (head->type == OP_PIPE || head->type == OP_SEMI) curr_state = q_cmd;
            else if (head->type == OP_AMP) curr_state = q_bg;
            else { curr_state = q_error; break; }
        }
        else if (curr_state == q_cmd) {
            if (head->type == WORD) curr_state = q_arg;
            else { curr_state = q_error; break; }
        }
        else if (curr_state == q_tgt) {
            if (head->type == WORD) curr_state = q_arg;
            else { curr_state = q_error; break; }
        }
        else if (curr_state == q_bg) {
            if (head->type == WORD) curr_state = q_arg;
            else { curr_state = q_error; break; }
        }
        head = head->next_token;
    }

    if (curr_state == q_error || curr_state == q_cmd || curr_state == q_tgt) {
        return 1;
    }
    return 0; // q_start, q_arg, q_bg are valid end states
}
