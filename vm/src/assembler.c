#include <stdbool.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "pvm/assembler.h"
#include "pvm/tokenizer.h"
#include "pvm/utils.h"
#include "pvm/symbol.h"
#include "pvm/vm.h"
#include "pvm/validator.h"

// -------------------------------------------------
// Globals
// -------------------------------------------------
token_line_t *tokens;
uint16_t machine_code[1024];
size_t machine_code_address = 0;
uint16_t origin = 0x3000; // default

// -------------------------------------------------
// Helper Functions
// -------------------------------------------------

static int register_id(const char *str)
{
    return strtol(str + 1, NULL, 10);
}

static bool is_register(const char *str)
{
    return str && (str[0] == 'R' || str[0] == 'r');
}

static void emit(uint16_t code)
{
    machine_code[machine_code_address++] = code;
}

// -------------------------------------------------
// Instruction Encoding Helpers
// -------------------------------------------------

uint16_t get_register_value(const char *reg)
{
    if (!is_register(reg))
        return 0xFFFF;
    return reg[1] - '0';
}

instruction_spec_t find_spec(const char *mnemonic);

// -------------------------------------------------
// Encoder Functions
// -------------------------------------------------

void encode_add(token_t *operands)
{
    instruction_spec_t spec = find_spec("ADD");
    uint16_t code = spec.opcode << 12;
    code |= register_id(operands[0].value) << 9;
    code |= register_id(operands[1].value) << 6;

    if (is_register(operands[2].value))
    {
        code |= register_id(operands[2].value);
    }
    else
    {
        int imm5 = parse_number(operands[2].value);
        code |= (1 << 5) | (imm5 & 0x1F);
    }
    emit(code);
}

void encode_and(token_t *operands)
{
    instruction_spec_t spec = find_spec("AND");

    uint16_t code = spec.opcode << 12;
    code |= register_id(operands[0].value) << 9;
    code |= register_id(operands[1].value) << 6;

    if (is_register(operands[2].value))
    {
        code |= register_id(operands[2].value);
    }
    else
    {
        int imm5 = parse_number(operands[2].value);
        code |= (1 << 5) | (imm5 & 0x1F);
    }
    emit(code);
}

void encode_ld(token_t *operands)
{
    instruction_spec_t spec = find_spec("LD");

    uint16_t code = spec.opcode << 12;
    code |= register_id(operands[0].value) << 9;
    uint16_t target = look_symbol(tokens, operands[1].value)->address;
    int16_t offset = sign_extend(target - (machine_code_address + 1), 9);
    code |= offset & 0x1FF;
    emit(code);
}

void encode_ldi(token_t *operands)
{
    instruction_spec_t spec = find_spec("LDI");

    uint16_t code = spec.opcode << 12;
    code |= register_id(operands[0].value) << 9;
    uint16_t target = look_symbol(tokens, operands[1].value)->address;
    int16_t offset = sign_extend(target - (machine_code_address + 1), 9);
    code |= offset & 0x1FF;
    emit(code);
}

void encode_st(token_t *operands)
{
    instruction_spec_t spec = find_spec("ST");

    uint16_t code = spec.opcode << 12;
    code |= register_id(operands[0].value) << 9;
    uint16_t target = look_symbol(tokens, operands[1].value)->address;
    int16_t offset = sign_extend(target - (machine_code_address + 1), 9);
    code |= offset & 0x1FF;
    emit(code);
}

void encode_brnzp(token_t *operands)
{
    instruction_spec_t spec = find_spec("BRnzp");
    uint16_t code = spec.opcode << 12 | 0x7 << 9;
    uint16_t target = look_symbol(tokens, operands[0].value)->address;
    int16_t offset = sign_extend(target - (machine_code_address + 1), 9);
    code |= offset & 0x1FF;
    emit(code);
}

void encode_brzp(token_t *operands)
{
    instruction_spec_t spec = find_spec("BRzp");
    uint16_t code = spec.opcode << 12 | 0x3 << 9;
    uint16_t target = look_symbol(tokens, operands[0].value)->address;
    int16_t offset = sign_extend(target - (machine_code_address + 1), 9);
    code |= offset & 0x1FF;
    emit(code);
}

void encode_getc(token_t *ops) { emit(0xF020); }
void encode_out(token_t *ops) { emit(0xF021); }
void encode_puts(token_t *ops) { emit(0xF022); }
void encode_in(token_t *ops) { emit(0xF023); }
void encode_putsp(token_t *ops) { emit(0xF024); }
void encode_halt(token_t *ops) { emit(0xF025); }
void encode_ret(token_t *ops) { emit(0xC1C0); }

void encode_org(token_t *ops)
{
    machine_code_address = parse_number(ops[0].value) - 0x3000;
}
void encode_fill(token_t *ops) { emit(parse_number(ops[0].value)); }
void encode_blkw(token_t *ops) { machine_code_address += parse_number(ops[0].value); }
void encode_end(token_t *ops) {} // Nothing needed

// -------------------------------------------------
// Instruction Specification
// -------------------------------------------------

instruction_spec_t instruction_table[] = {
    {"ADD", 0x1, {TYPE_REG, TYPE_REG, TYPE_REG}, 3, true, false, encode_add},
    {"AND", 0x5, {TYPE_REG, TYPE_REG, TYPE_REG}, 3, true, false, encode_and},
    {"NOT", 0x9, {TYPE_REG, TYPE_REG}, 2, false, false, encode_add},

    {"BRnzp", 0x0, {TYPE_LABEL}, 1, true, false, encode_brnzp},
    {"BRzp", 0x0, {TYPE_LABEL}, 1, true, false, encode_brzp},

    {"LD", 0x2, {TYPE_REG, TYPE_LABEL}, 2, true, false, encode_ld},
    {"LDI", 0xA, {TYPE_REG, TYPE_LABEL}, 2, true, false, encode_ldi},
    {"ST", 0x3, {TYPE_REG, TYPE_LABEL}, 2, true, false, encode_st},

    {"RET", 0xC, {}, 0, false, false, encode_ret},
    {"HALT", 0xF, {}, 0, false, false, encode_halt},
    {"GETC", 0xF, {}, 0, false, false, encode_getc},
    {"OUT", 0xF, {}, 0, false, false, encode_out},
    {"PUTS", 0xF, {}, 0, false, false, encode_puts},
    {"IN", 0xF, {}, 0, false, false, encode_in},
    {"PUTSP", 0xF, {}, 0, false, false, encode_putsp},

    {".ORIG", 0x0, {TYPE_NUMBER}, 1, false, true, encode_org},
    {".FILL", 0x0, {TYPE_NUMBER}, 1, false, true, encode_fill},
    {".BLKW", 0x0, {TYPE_NUMBER}, 1, false, true, encode_blkw},
    {".END", 0x0, {}, 0, false, true, encode_end}};

int instruction_spec_size = sizeof(instruction_table) / sizeof(instruction_table[0]);

instruction_spec_t find_spec(const char *mnemonic)
{
    for (int i = 0; i < instruction_spec_size; i++)
    {
        if (strcmp(mnemonic, instruction_table[i].mnemonic) == 0)
            return instruction_table[i];
    }
    instruction_spec_t invalid = {"", 0, {TYPE_NO_OPERAND}, 0, false, false, NULL};
    return invalid;
}

// -------------------------------------------------
// Assembler Driver
// -------------------------------------------------

void encode_instruction(const char *mnemonic, token_t *operands)
{
    instruction_spec_t spec = find_spec(mnemonic);
    if (!spec.encode_fn)
    {
        report_error(1, "Unknown opcode: %s", mnemonic);
    }
    spec.encode_fn(operands);
}

void handle_orig(token_line_t *line)
{
    if (line->instr->opcode.type == TOKEN_DIRECTIVE)
    {
        origin = parse_number(line->instr->opcode.value);
        // set_current_address(origin);
    }
}

void encode_line(token_line_t *line)
{
    if (is_instruction(line->instr->opcode.value))
    {
        encode_instruction(line->instr->opcode.value, line->instr->operands);
    }
    else if (strcmp(line->instr->opcode.value, ".ORIG") == 0)
    {
        handle_orig(line);
    }
    else
    {
        report_error(line->line_count, "Unknown directive or opcode");
    }
}
