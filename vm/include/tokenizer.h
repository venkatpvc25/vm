#ifndef TOKENIZER_H
#define TOKENIZER_H

#define MAX_TOKENS_PER_LINE 10
#define MAX_LINES 1024

typedef enum
{
    TOKEN_LABEL,
    TOKEN_OPCODE,
    TOKEN_REGISTER,
    TOKEN_IMMEDIATE,
    TOKEN_DIRECTIVE,
    TOKEN_STRING,
    TOKEN_COMMENT,
    TOKEN_IDENTIFIER,
    TOKEN_UNKNOWN
} TokenType;

typedef struct
{
    TokenType type;
    char lexeme[64];
} Token;

typedef struct
{
    Token tokens[MAX_TOKENS_PER_LINE];
    int token_count;
} TokenLine;

int tokenize_program(const char *program_text, TokenLine *lines);

void print_token(const Token *t);
void print_token_line(const TokenLine *lines, int line_count);

#endif
