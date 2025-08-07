
#ifndef VM_H
#define VM_H

#include <stdint.h>
#include <stdlib.h>

#include "assembler.h"

#define MAX_STACK_SIZE (1 << 16)

typedef enum
{
    R_R0 = 0,
    R_R1,
    R_R2,
    R_R3,
    R_R4,
    R_R5,
    R_R6,
    R_R7,
    R_PC,
    R_COND,
    R_COUNT
} Registers;

typedef struct
{
    uint16_t mem[MAX_STACK_SIZE];
    uint16_t reg[R_COUNT];
} VM;

void load_program(VM *vm, uint16_t *instructions, size_t count);
void run(VM *vm);

#endif