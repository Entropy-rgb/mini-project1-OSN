#include <stdio.h>
#include <stdlib.h>
#include "token.h"

enum parser_states
{
    q0,     // start
    q1,     // in command
    q2,     // accept filename , redirection
    q3,     // except command , we just saw a pipe, a command must follow
    q4,     // after seperator , i.e ; or &
    qerror, // invalid grammar found
};

int parser(token *head)
{
    enum parser_states curr_state = q0;
    while (head != NULL)
    {
        if (curr_state == q0)
        {
            if (head->type == WORD)
            {
                curr_state = q1;
                head = head->next_token;
                continue;
            }
            else if (head->type == OP_GT || head->type == OP_GTGT || head->type == OP_LT || head->type == OP_LTLT)
            {
                curr_state = q2;
                head = head->next_token;
                continue;
            }
            else if (head->type == OP_PIPE || head->type == OP_SEMI || head->type == OP_AMP)
            {
                curr_state = qerror;
                break;
            }
        }
        else if (curr_state == q1)
        {
            if (head->type == WORD)
            {
                curr_state = q1;
                head = head->next_token;
                continue;
            }
            else if (head->type == OP_GT || head->type == OP_GTGT || head->type == OP_LT || head->type == OP_LTLT)
            {
                curr_state = q2;
                head = head->next_token;
                continue;
            }
            else if (head->type == OP_PIPE)
            {
                curr_state = q3;
                head = head->next_token;
                continue;
            }
            else if (head->type == OP_SEMI || head->type == OP_AMP)
            {
                curr_state = q4;
                head = head->next_token;
                continue;
            }
        }
        else if (curr_state == q2)
        {
            if (head->type == WORD)
            {
                curr_state = q1;
                head = head->next_token;
                continue;
            }
            else if (head->type == OP_GT || head->type == OP_GTGT || head->type == OP_LT || head->type == OP_LTLT || head->type == OP_PIPE || head->type == OP_SEMI || head->type == OP_AMP)
            {
                curr_state = qerror;
                break;
            }
        }
        else if (curr_state == q3)
        {
            if (head->type == WORD)
            {
                curr_state = q1;
                head = head->next_token;
                continue;
            }
            else if (head->type == OP_GT || head->type == OP_GTGT || head->type == OP_LT || head->type == OP_LTLT)
            {
                curr_state = q2;
                head = head->next_token;
                continue;
            }
            else
            {
                curr_state = qerror;
                break;
            }
        }
        else if (curr_state == q4)
        {
            if (head->type == WORD)
            {
                curr_state = q1;
                head = head->next_token;
                continue;
            }
            else if (head->type == OP_GT || head->type == OP_GTGT || head->type == OP_LT || head->type == OP_LTLT)
            {
                curr_state = q2;
                head = head->next_token;
                continue;
            }
            else
            {
                curr_state = qerror;
                break;
            }
        }
        else
        {
            break;
        }
    }

    if (curr_state == qerror || curr_state == q2 || curr_state == q3)
    {
        return 1;
    }
    return 0;
}