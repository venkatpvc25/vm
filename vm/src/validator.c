#include <stdbool.h>
#include <limits.h>

#include "pvm/assembler.h"
#include "pvm/error.h"

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

        if (!spec.mnemonic)
        {
            report_error(instr.line_number, "Unknown instruction \"%s\"", instr.opcode.value);
            return false;
        }

        int expected_operands = spec.operand_count;

        for (int j = 0; j < expected_operands; j++)
        {
            if (!validate_operand(spec.operand_types[j], instr.operands[j].value))
            {
                report_error(
                    instr.line_number,
                    "Invalid operand \"%s\" at position %d for instruction \"%s\"\n",
                    instr.operands[j].value, j + 1, spec.mnemonic);
                return false;
            }
        }
    }

    return true;
}
