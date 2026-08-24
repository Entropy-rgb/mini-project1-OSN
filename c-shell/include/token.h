#ifndef TOKEN_H
#define TOKEN_H

typedef enum TokenType {
    OP_PIPE, // |
    OP_AMP,  // &
    OP_SEMI, // ;
    OP_LT,   // <
    OP_LTLT, // <<
    OP_GT,   // >
    OP_GTGT, // >>
    WORD,    // normal word
} TokenType ;

typedef struct token {
    char* content;
    enum TokenType type;
    struct token* next_token;
} token;

#endif