#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <stdint.h>

#include "tokenizer.h"

#define MAX_MACHINE_CODE 1024

typedef struct machine_code
{
    uint16_t address;
    uint16_t instruction;
} machine_code_t;

typedef struct machine_code_array
{
    machine_code_t machine_code[MAX_MACHINE_CODE];
    size_t machine_code_count;
} machine_code_array_t;

int assemble(token_line_t *lines);

#endif
