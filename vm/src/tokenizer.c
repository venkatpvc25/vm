#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include "tokenizer.h"

#define SYMBOL_MAX_LENGTH 64

const char symbols[SYMBOL_MAX_LENGTH];

const char *lc3_opcodes[] = {
    "ADD", "AND",
    "BR", "BRn", "BRz", "BRp", "BRnz", "BRzp", "BRnp", "BRnzp",
    "JMP", "JSR", "JSRR",
    "LD", "LDI", "LDR", "LEA",
    "NOT", "RET", "RTI",
    "ST", "STI", "STR",
    "TRAP"};

const int lc3_opcode_count = sizeof(lc3_opcodes) / sizeof(lc3_opcodes[0]);

static inline bool is_label(const char *str)
{
    if (!is_instruction(str) || !is_comment(str))
        return true;
    return false;
}

static inline bool is_instruction(const char *instr)
{
    for (int i = 0; i < lc3_opcode_count; i++)
    {
        if (strcmp(instr, lc3_opcodes[i]))
        {
            return true;
        }
    }
    return false;
}

static inline bool is_comment(const char *cmnt)
{
    return cmnt[0] == ';';
}

void tokenizer(const char *code)
{
}