#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <stdlib.h>
#include <stdint.h>

#define MAX_TOKENS_PER_LINE 10
#define MAX_LINES 1024
#define MAX_SYMBOLS 64

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
    TOKEN_HEX,
    TOKEN_NUMBER,
    TOKEN_COMMA,
    TOKEN_UNKNOWN
} token_type;

typedef struct symbol_entry symbol_entry_t;

typedef struct token
{
    token_type type;
    char lexeme[64];
} token_t;

typedef struct token_array
{
    token_t tokens[MAX_TOKENS_PER_LINE];
    size_t token_count;
    symbol_entry_t *symbols[MAX_SYMBOLS];
    size_t symbol_count;
} token_array_t;

void tokenizer(const char *line, token_array_t *tokens);
const char *token_type_to_string(token_type type);

#endif
