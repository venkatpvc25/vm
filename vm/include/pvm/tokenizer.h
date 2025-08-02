#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <stdlib.h>
#include <stdint.h>

#define MAX_TOKENS_PER_LINE 5
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

typedef enum
{
    TYPE_NONE,
    TYPE_NUMBER,
    TYPE_STRING,
    TYPE_REG,
    TYPE_IMM5,
    TYPE_IMM6,
    TYPE_IMM8,
    TYPE_IMM16,
    TYPE_ADDRESS,
    TYPE_IMMEDIATE,
    TYPE_OFFSET6,
    TYPE_PCOFFSET9,
    TYPE_PCOFFSET11,
    TYPE_BASE_OFFSET6,
    TYPE_LABEL,
    TYPE_TRAPVEC,
    TYPE_BASE_REG,
    TYPE_NO_OPERAND
} operand_type;

typedef struct symbol_entry symbol_entry_t;

typedef struct token
{
    token_type type;
    operand_type operand_type;
    char value[64];
} token_t;

typedef struct instr
{
    token_t opcode;
    token_t operands[MAX_TOKENS_PER_LINE];
    int operand_count;
    int line_number;
} instr_t;

typedef struct token_line
{
    instr_t instr[MAX_LINES];
    int line_count;
    symbol_entry_t *symbols[MAX_SYMBOLS];
    size_t symbol_count;
} token_line_t;

extern token_line_t token_lines;

// typedef struct token_line
// {
//     token_t opcode;
//     token_t operands[MAX_TOKENS_PER_LINE];
//     int operand_count;
//     int line_count;
//     symbol_entry_t *symbols[MAX_SYMBOLS];
//     size_t symbol_count;
// } token_line_t;

void tokenizer(const char *line);
const char *token_type_to_string(token_type type);
void print_token_line();
#endif
