#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <stdint.h>

#include "tokenizer.h"

#define MAX_MACHINE_CODE 1024

#define MAX_TOKENS 6

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

typedef struct
{
    const char *mnemonic;
    uint8_t opcode;
    operand_type operand_types[MAX_TOKENS];
    int operand_count;
    bool supports_immediate;
    bool is_pseudo;
    void (*encode_fn)(token_t *operands);
} instruction_spec_t;

int assemble();
instruction_spec_t find_spec(const char *mnemonic);

#endif
