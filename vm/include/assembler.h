#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <stdint.h>

#define MAX_TOKENS_PER_LINE 10
#define MAX_LINES 1024
#define MAX_LABELS 256

typedef enum
{
    TOKEN_LABEL,
    TOKEN_OPCODE,
    TOKEN_REGISTER,
    TOKEN_IMMEDIATE,
    TOKEN_DIRECTIVE,
    TOKEN_COMMENT,
    TOKEN_IDENTIFIER
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

typedef struct
{
    uint16_t address;
    uint16_t instruction;
} MachineCode;

int assemble(TokenLine *lines, int line_count, MachineCode *output);

#endif
