#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <stdint.h>

#define MAX_TOKENS_PER_LINE 10
#define MAX_LINES 1024
#define MAX_LABELS 256

typedef struct
{
    uint16_t address;
    uint16_t instruction;
} MachineCode;

// int assemble(TokenLine *lines, int line_count, MachineCode *output);

#endif
