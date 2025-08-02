#include <stdbool.h>

#include "pvm/assembler.h"
#include "pvm/tokenizer.h"
#include <limits.h>

#define MAX_TOKENS 6

typedef struct
{
    const char *mnemonic;                   // e.g., "ADD", "LD", "BR"
    uint8_t opcode;                         // 4-bit LC-3 opcode (e.g., 0001 for ADD)
    operand_type operand_types[MAX_TOKENS]; // Expected operand types (e.g., REG, IMM5)
    int operand_count;                      // Number of operands
    bool supports_immediate;                // e.g., ADD R1, R2, #5
    bool is_pseudo;                         // If it's a pseudo-op (.ORIG, .FILL, etc.)
} instruction_spec_t;

instruction_spec_t instruction_table[] = {
    // ALU instructions
    {"ADD", 0x1, {TYPE_REG, TYPE_REG, TYPE_REG}, 3, true, false},
    {"AND", 0x5, {TYPE_REG, TYPE_REG, TYPE_REG}, 3, true, false},
    {"NOT", 0x9, {TYPE_REG, TYPE_REG, TYPE_NONE}, 2, false, false},

    // Branch instructions (all map to opcode 0x0 with condition flags)
    {"BR", 0x0, {TYPE_LABEL, TYPE_NONE, TYPE_NONE}, 1, true, false},
    {"BRn", 0x0, {TYPE_LABEL, TYPE_NONE, TYPE_NONE}, 1, true, false},
    {"BRz", 0x0, {TYPE_LABEL, TYPE_NONE, TYPE_NONE}, 1, true, false},
    {"BRp", 0x0, {TYPE_LABEL, TYPE_NONE, TYPE_NONE}, 1, true, false},
    {"BRnz", 0x0, {TYPE_LABEL, TYPE_NONE, TYPE_NONE}, 1, true, false},
    {"BRnp", 0x0, {TYPE_LABEL, TYPE_NONE, TYPE_NONE}, 1, true, false},
    {"BRzp", 0x0, {TYPE_LABEL, TYPE_NONE, TYPE_NONE}, 1, true, false},
    {"BRnzp", 0x0, {TYPE_LABEL, TYPE_NONE, TYPE_NONE}, 1, true, false},

    // Jumps
    {"JMP", 0xC, {TYPE_BASE_REG, TYPE_NONE, TYPE_NONE}, 1, false, false},
    {"JSR", 0x4, {TYPE_LABEL, TYPE_NONE, TYPE_NONE}, 1, true, false},
    {"JSRR", 0x4, {TYPE_BASE_REG, TYPE_NONE, TYPE_NONE}, 1, false, false},

    // Memory operations
    {"LD", 0x2, {TYPE_REG, TYPE_LABEL, TYPE_NONE}, 2, true, false},
    {"LDI", 0xA, {TYPE_REG, TYPE_LABEL, TYPE_NONE}, 2, true, false},
    {"LDR", 0x6, {TYPE_REG, TYPE_BASE_REG, TYPE_OFFSET6}, 3, false, false},
    {"LEA", 0xE, {TYPE_REG, TYPE_LABEL, TYPE_NONE}, 2, true, false},
    {"ST", 0x3, {TYPE_REG, TYPE_LABEL, TYPE_NONE}, 2, true, false},
    {"STI", 0xB, {TYPE_REG, TYPE_LABEL, TYPE_NONE}, 2, true, false},
    {"STR", 0x7, {TYPE_REG, TYPE_BASE_REG, TYPE_OFFSET6}, 3, false, false},

    // TRAP routines
    {"TRAP", 0xF, {TYPE_TRAPVEC, TYPE_NONE, TYPE_NONE}, 1, false, false},
    {"HALT", 0xF, {TYPE_NONE, TYPE_NONE, TYPE_NONE}, 0, false, false},
    {"GETC", 0xF, {TYPE_NONE, TYPE_NONE, TYPE_NONE}, 0, false, false},
    {"OUT", 0xF, {TYPE_NONE, TYPE_NONE, TYPE_NONE}, 0, false, false},
    {"PUTS", 0xF, {TYPE_NONE, TYPE_NONE, TYPE_NONE}, 0, false, false},
    {"IN", 0xF, {TYPE_NONE, TYPE_NONE, TYPE_NONE}, 0, false, false},
    {"PUTSP", 0xF, {TYPE_NONE, TYPE_NONE, TYPE_NONE}, 0, false, false},

    // Pseudo-ops (assembler directives)
    {".ORIG", 0x00, {TYPE_NUMBER, TYPE_NONE, TYPE_NONE}, 1, false, true},
    {".FILL", 0x00, {TYPE_NUMBER, TYPE_NONE, TYPE_NONE}, 1, false, true},
    {".BLKW", 0x00, {TYPE_NUMBER, TYPE_NONE, TYPE_NONE}, 1, false, true},
    {".STRINGZ", 0x00, {TYPE_STRING, TYPE_NONE, TYPE_NONE}, 1, false, true},
    {".END", 0x00, {TYPE_NONE, TYPE_NONE, TYPE_NONE}, 0, false, true}};

int instruction_spec_size = sizeof(instruction_table) / sizeof(instruction_table[0]);

instruction_spec_t find_spec(const char *opcode)
{
    instruction_spec_t INVALID_SPEC = {"", 0x00, {TYPE_NO_OPERAND}, 0, false, false};
    for (int i = 0; i < instruction_spec_size; i++)
    {
        if (strcmp(opcode, instruction_table[i].mnemonic) == 0)
        {
            return instruction_table[i];
        }
    }
    return INVALID_SPEC;
}

bool is_valid_register(const char *str)
{
    return str && strlen(str) == 2 && str[0] == 'R' && str[1] >= '0' && str[1] <= '7';
}

bool is_valid_imm5(const char *str)
{
    if (!str || str[0] != '#')
        return false;
    int val = atoi(str + 1);
    return val >= -16 && val <= 15;
}

bool is_valid_offset6(const char *str)
{
    if (!str || str[0] != '#')
        return false;
    int val = atoi(str + 1);
    return val >= -32 && val <= 31;
}

bool is_valid_pc_offset9(const char *str)
{
    if (!str)
        return false;
    if (str[0] == '#')
    {
        int val = atoi(str + 1);
        return val >= -256 && val <= 255;
    }
    return true; // Assume it's a label
}

bool is_valid_pc_offset11(const char *str)
{
    if (!str)
        return false;
    if (str[0] == '#')
    {
        int val = atoi(str + 1);
        return val >= -1024 && val <= 1023;
    }
    return true; // label
}

bool is_valid_base_reg(const char *str)
{
    return is_valid_register(str);
}

bool is_valid_trapvect8(const char *str)
{
    if (!str || str[0] != 'x')
        return false;
    int val = (int)strtol(str + 1, NULL, 16);
    return val >= 0x00 && val <= 0xFF;
}

bool is_valid_label(const char *str)
{
    if (!str || !isalpha(str[0]))
        return false;
    for (const char *p = str; *p; ++p)
    {
        if (!isalnum(*p) && *p != '_')
            return false;
    }
    return true;
}

bool is_valid_string(const char *str)
{
    return str && str[0] == '"' && str[strlen(str) - 1] == '"';
}

bool is_valid_number(const char *str)
{
    if (!str)
        return false;
    if (str[0] == '#')
    {
        int val = atoi(str + 1);
        return val >= SHRT_MIN && val <= SHRT_MAX;
    }
    else if (str[0] == 'x' || str[0] == 'X')
    {
        int val = (int)strtol(str + 1, NULL, 16);
        return val >= 0 && val <= 0xFFFF;
    }
    return false;
}

bool validate_operand(operand_type type, const char *value)
{
    switch (type)
    {
    case TYPE_REG:
        return is_valid_register(value);
    case TYPE_IMM5:
        return is_valid_imm5(value);
    case TYPE_OFFSET6:
        return is_valid_offset6(value);
    case TYPE_PCOFFSET9:
        return is_valid_pc_offset9(value);
    case TYPE_PCOFFSET11:
        return is_valid_pc_offset11(value);
    case TYPE_BASE_REG:
        return is_valid_base_reg(value);
    case TYPE_TRAPVEC:
        return is_valid_trapvect8(value);
    case TYPE_LABEL:
        return is_valid_label(value);
    case TYPE_STRING:
        return is_valid_string(value);
    case TYPE_NUMBER:
        return is_valid_number(value);
    case TYPE_NONE:
        return true;
    }
    return false;
}

bool validate_instruction(token_line_t *tokens)
{

    for (int i = 0; i < tokens->line_count; i++)
    {
        instr_t instr = tokens->instr[i];
        instruction_spec_t spec = find_spec(instr.opcode.value);
        int expected_operands = spec.operand_count;
        int token_index = 1;
        int operand_index = 0;
        while (operand_index < expected_operands)
        {
            if (!validate_operand(spec.operand_types[operand_index], instr.operands[operand_index].value))
            {
                printf("Invalid operand at position %d for instruction %s\n", operand_index + 1, spec.mnemonic);
                return false;
            }
            token_index++;
            operand_index++;
        }
    }

    return true;
}

int assemble(token_line_t *tokens)
{
    validate_instruction(tokens);
}