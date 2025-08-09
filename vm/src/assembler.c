// assembler.c
#include <stdbool.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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
// Config / Types
// -------------------------------------------------
#define DEFAULT_ORIGIN 0x3000
#define MAX_OUTPUT_WORDS 1024
#define MAX_SIZE 65536

// -------------------------------------------------
// Helper Functions (context-aware)
// -------------------------------------------------

static bool is_register(const char *str)
{
    return str && (str[0] == 'R' || str[0] == 'r');
}

static void emit(segment_t **ctx, uint16_t code)
{
    if ((*ctx)->pos >= MAX_SIZE)
    {
        report_error(0, "Output buffer overflow while emitting code (pos=%zu, size=%zu)\n", (*ctx)->pos, (*ctx)->size);
        return;
    }
    (*ctx)->data[(*ctx)->pos++] = code;
}

static int16_t pc_relative_offset(segment_t *ctx, uint16_t target, int bit_count)
{
    int32_t pc = (int32_t)ctx->origin + (int32_t)ctx->pos;
    int32_t diff = (int32_t)target - (pc + 1);
    return sign_extend(diff, bit_count);
}

static bool is_number(const char *str) { return str[0] == '#' || str[0] == 'x' || str[0] == 'X'; }

int parse_imm_or_label(segment_t *ctx, token_line_t *tokens, size_t idx,
                       token_t operand, int bit_width, int16_t *out_value)
{
    if (is_number(operand.value))
    {
        long val = parse_number(operand.value);
        if (val < -(1 << (bit_width - 1)) || val >= (1 << (bit_width - 1)))
        {
            report_error(idx + 1, "Immediate out of range for %d bits: %ld", bit_width, val);
            emit(ctx, 0);
            return 0;
        }
        *out_value = (int16_t)val;
        return 1;
    }

    symbol_entry_t *sym = look_symbol(tokens, operand.value);
    if (!sym || sym->address == (uint16_t)-1)
    {
        report_error(idx + 1, "Undefined symbol: %s\n", operand.value);
        emit(ctx, 0);
        return 0;
    }

    int16_t offset = pc_relative_offset(ctx, sym->address, bit_width);
    *out_value = offset;
    return 1;
}

char *strip_quotes(char *str)
{
    size_t len;

    if (!str)
        return NULL;

    len = strlen(str);
    if (len >= 2)
    {
        char first = str[0];
        char last = str[len - 1];

        if ((first == '"' && last == '"') ||
            (first == '\'' && last == '\''))
        {
            str[len - 1] = '\0';
            return str + 1;
        }
    }
    return str;
}

// -------------------------------------------------
// Instruction Encoding Helpers
// -------------------------------------------------

uint16_t parse_register(const char *reg)
{
    if (!is_register(reg))
        return 0xFFFF;
    return (reg[1] - '0') & 0x7;
}

instruction_spec_t find_spec(const char *mnemonic);

// -------------------------------------------------
// Encoder Functions (all signatures unified)
// -------------------------------------------------

// ADD   DR, SR1, SR2          ; DR <- SR1 + SR2  (reg mode)
// ADD   DR, SR1, imm5         ; DR <- SR1 + imm5 (imm5 is signed, 5 bits)
void encode_add(segment_t **ctx, token_line_t *tokens, size_t idx)
{
    token_t *ops = tokens->instr[idx].operands;
    uint16_t dr = parse_register(ops[0].value) << 9;
    uint16_t sr1 = parse_register(ops[1].value) << 6;

    uint16_t code = (0x1 << 12) | dr | sr1;

    if (is_register(ops[2].value))
    {
        uint16_t sr2 = parse_register(ops[2].value);
        code |= sr2 & 0x7;
    }
    else
    {
        code |= (1 << 5) | (parse_number(ops[2].value) & 0x1F);
    }

    emit(ctx, code);
}

// AND DR, SR1, SR2            ; DR <- SR1 & SR2
// AND DR, SR1, imm5           ; DR <- SR1 & imm5
void encode_and(segment_t **ctx, token_line_t *tokens, size_t idx)
{
    token_t *ops = tokens->instr[idx].operands;
    uint16_t dr = parse_register(ops[0].value) << 9;
    uint16_t sr1 = parse_register(ops[1].value) << 6;

    uint16_t code = (0x5 << 12) | dr | sr1; // FIX: opcode = 0x5

    if (is_register(ops[2].value))
    {
        uint16_t sr2 = parse_register(ops[2].value);
        code |= sr2 & 0x7;
    }
    else
    {
        code |= (1 << 5) | (parse_number(ops[2].value) & 0x1F);
    }

    emit(ctx, code);
}

// LD DR, LABEL/offset9         ; DR <- MEM[PC + offset9]
void encode_ld(segment_t **ctx, token_line_t *tokens, size_t idx)
{
    token_t *ops = tokens->instr[idx].operands;
    uint16_t dr = parse_register(ops[0].value) << 9;
    int16_t offset;
    if (!parse_imm_or_label(*ctx, tokens, idx, ops[1], 9, &offset))
        return;

    uint16_t code = (0x2 << 12) | dr | (offset & 0x1FF);
    emit(ctx, code);
}

// LDI DR, LABEL/offset9        ; DR <- MEM[ MEM[PC + offset9] ]
void encode_ldi(segment_t **ctx, token_line_t *tokens, size_t idx)
{
    token_t *ops = tokens->instr[idx].operands;
    uint16_t dr = parse_register(ops[0].value) << 9;
    int16_t offset;
    if (!parse_imm_or_label(*ctx, tokens, idx, ops[1], 9, &offset))
        return;

    uint16_t code = (0xA << 12) | dr | (offset & 0x1FF);
    emit(ctx, code);
}

// ST SR, LABEL/offset9         ; MEM[PC + offset9] <- SR
void encode_st(segment_t **ctx, token_line_t *tokens, size_t idx)
{
    token_t *ops = tokens->instr[idx].operands;
    uint16_t sr = parse_register(ops[0].value) << 9;
    int16_t offset;
    if (!parse_imm_or_label(*ctx, tokens, idx, ops[1], 9, &offset))
        return;

    uint16_t code = (0x3 << 12) | sr | (offset & 0x1FF);
    emit(ctx, code);
}

// LDR DR, BaseR, offset6       ; DR <- MEM[BaseR + offset6]
void encode_ldr(segment_t **ctx, token_line_t *tokens, size_t idx)
{
    token_t *ops = tokens->instr[idx].operands;
    uint16_t dr = parse_register(ops[0].value) << 9;
    uint16_t baseR = parse_register(ops[1].value) << 6;

    int16_t offset;
    if (!parse_imm_or_label(*ctx, tokens, idx, ops[2], 6, &offset))
        return;

    uint16_t code = (0x6 << 12) | dr | baseR | (offset & 0x3F);
    emit(ctx, code);
}

// STI SR, LABEL/offset9        ; MEM[ MEM[PC + offset9] ] <- SR
void encode_sti(segment_t **ctx, token_line_t *tokens, size_t idx)
{
    token_t *ops = tokens->instr[idx].operands;
    uint16_t sr = parse_register(ops[0].value) << 9;
    int16_t offset;
    if (!parse_imm_or_label(*ctx, tokens, idx, ops[1], 9, &offset))
        return;

    uint16_t code = (0xB << 12) | sr | (offset & 0x1FF);
    emit(ctx, code);
}

// LEA DR, LABEL/offset9        ; DR <- PC + offset9
void encode_lea(segment_t **ctx, token_line_t *tokens, size_t idx)
{
    token_t *ops = tokens->instr[idx].operands;
    uint16_t dr = parse_register(ops[0].value) << 9;
    int16_t offset;
    if (!parse_imm_or_label(*ctx, tokens, idx, ops[1], 9, &offset))
        return;

    uint16_t code = (0xE << 12) | dr | (offset & 0x1FF);
    emit(ctx, code);
}

// NOT DR, SR  ; DR <- NOT(SR)
void encode_not(segment_t **ctx, token_line_t *tokens, size_t idx)
{
    token_t *operands = tokens->instr[idx].operands;
    uint16_t dr = parse_register(&operands[0].value) << 9;
    uint16_t sr = parse_register(&operands[1].value) << 6;

    uint16_t code = (0x9 << 12) | dr | sr | 0x3F;

    emit(ctx, code);
}

// JSR LABEL/offset11           ; R7 <- PC; PC <- PC + offset11
void encode_jsr(segment_t **ctx, token_line_t *tokens, size_t idx)
{
    token_t *ops = tokens->instr[idx].operands;
    int16_t offset;
    if (!parse_imm_or_label(*ctx, tokens, idx, ops[0], 11, &offset))
        return;

    uint16_t code = (0x4 << 12) | (1 << 11) | (offset & 0x7FF);
    emit(ctx, code);
}

// BRnzp LABEL                  ; PC <- PC + offset9 (always branch)
void encode_brnzp(segment_t **ctx, token_line_t *tokens, size_t idx)
{
    int16_t offset;
    if (!parse_imm_or_label(*ctx, tokens, idx, tokens->instr[idx].operands[0], 9, &offset))
        return;

    uint16_t code = (0x0 << 12) | (0x7 << 9) | (offset & 0x1FF);
    emit(ctx, code);
}

// BRz LABEL                    ; PC <- PC + offset9 if Z
void encode_brz(segment_t **ctx, token_line_t *tokens, size_t idx)
{
    int16_t offset;
    if (!parse_imm_or_label(*ctx, tokens, idx, tokens->instr[idx].operands[0], 9, &offset))
        return;

    uint16_t code = (0x0 << 12) | (0x2 << 9) | (offset & 0x1FF);
    emit(ctx, code);
}

// BRzp LABEL                   ; PC <- PC + offset9 if Z or P
void encode_brzp(segment_t **ctx, token_line_t *tokens, size_t idx)
{
    int16_t offset;
    if (!parse_imm_or_label(*ctx, tokens, idx, tokens->instr[idx].operands[0], 9, &offset))
        return;

    uint16_t code = (0x0 << 12) | (0x3 << 9) | (offset & 0x1FF);
    emit(ctx, code);
}

// Generic BR (default nzp=111 if no condition specified)
void encode_br(segment_t **ctx, token_line_t *tokens, size_t idx)
{
    int16_t offset;
    if (!parse_imm_or_label(*ctx, tokens, idx, tokens->instr[idx].operands[0], 9, &offset))
        return;

    uint16_t code = (0x0 << 12) | (0x7 << 9) | (offset & 0x1FF);
    emit(ctx, code);
}

void encode_getc(segment_t **ctx, token_line_t *tokens, size_t idx)
{
    (void)tokens;
    (void)idx;
    emit(ctx, 0xF020);
}
void encode_out(segment_t **ctx, token_line_t *tokens, size_t idx)
{
    (void)tokens;
    (void)idx;
    emit(ctx, 0xF021);
}
void encode_puts(segment_t **ctx, token_line_t *tokens, size_t idx)
{
    (void)tokens;
    (void)idx;
    emit(ctx, 0xF022);
}
void encode_in(segment_t **ctx, token_line_t *tokens, size_t idx)
{
    (void)tokens;
    (void)idx;
    emit(ctx, 0xF023);
}
void encode_putsp(segment_t **ctx, token_line_t *tokens, size_t idx)
{
    (void)tokens;
    (void)idx;
    emit(ctx, 0xF024);
}
void encode_halt(segment_t **ctx, token_line_t *tokens, size_t idx)
{
    (void)tokens;
    (void)idx;
    emit(ctx, 0xF025);
}
void encode_ret(segment_t **ctx, token_line_t *tokens, size_t idx)
{
    (void)tokens;
    (void)idx;
    emit(ctx, 0xC1C0);
}

// Directives
// segment_t *encode_org(segment_t **ctx, token_line_t *tokens, size_t idx)
// {
//     token_t *ops = tokens->instr[idx].operands;
//     uint16_t addr = parse_number(ops[0].value);

//     return add_segment(ctx, addr);
// }

void encode_fill(segment_t **ctx, token_line_t *tokens, size_t idx)
{
    token_t *ops = tokens->instr[idx].operands;
    uint16_t value = (uint16_t)parse_number(ops[0].value);
    emit(ctx, value);
}

void encode_blkw(segment_t **ctx, token_line_t *tokens, size_t idx)
{
    token_t *ops = tokens->instr[idx].operands;
    size_t count = (size_t)parse_number(ops[0].value);
    if ((*ctx)->pos + count > (*ctx)->size)
    {
        report_error(idx + 1, ".BLKW exceeds output buffer\n");
        (*ctx)->pos = (*ctx)->size;
        return;
    }
    for (size_t i = 0; i < count; ++i)
        emit(ctx, 0);
}

void encode_trap(segment_t **ctx, token_line_t *tokens, size_t idx)
{
    token_t *ops = tokens->instr[idx].operands;

    int16_t offset;
    if (!parse_imm_or_label(ctx, tokens, idx, tokens->instr[idx].operands[0], 8, &offset))
        return;
    uint16_t instr = 0xff << 12 | 0x0 << 9 | offset & 0xff;
    emit(ctx, instr);
}

void encode_stringz(segment_t **ctx, token_line_t *tokens, size_t idx)
{
    token_t *ops = tokens->instr[idx].operands;
    const char *str = strip_quotes(ops[0].value);
    for (size_t i = 0; i < strlen(str); i++)
    {
        emit(ctx, (uint16_t)str[i]);
    }
    emit(ctx, 0);
}

void encode_end(segment_t **ctx, token_line_t *tokens, size_t idx)
{
    (void)ctx;
    (void)tokens;
    (void)idx;
    // nothing to do for .END in this simple assembler
}

// -------------------------------------------------
// Instruction Specification Table
// -------------------------------------------------
// NOTE: ensure your instruction_spec_t in headers uses encode_fn_t or is compatible.
instruction_spec_t instruction_table[] = {
    // Arithmetic and logic
    {"ADD", 0x1, {TYPE_REG, TYPE_REG, TYPE_REG}, 3, true, false, (encode_fn_t)encode_add},
    {"AND", 0x5, {TYPE_REG, TYPE_REG, TYPE_REG}, 3, true, false, (encode_fn_t)encode_and},
    {"NOT", 0x9, {TYPE_REG, TYPE_REG}, 2, false, false, (encode_fn_t)encode_not},

    // Branches
    {"BR", 0x0, {TYPE_LABEL}, 1, true, false, (encode_fn_t)encode_br}, // unconditional
    {"BRz", 0x0, {TYPE_LABEL}, 1, true, false, (encode_fn_t)encode_brz},
    {"BRnzp", 0x0, {TYPE_LABEL}, 1, true, false, (encode_fn_t)encode_brnzp},
    {"BRzp", 0x0, {TYPE_LABEL}, 1, true, false, (encode_fn_t)encode_brzp},

    // Load / store
    {"LD", 0x2, {TYPE_REG, TYPE_LABEL}, 2, true, false, (encode_fn_t)encode_ld},
    {"LDI", 0xA, {TYPE_REG, TYPE_LABEL}, 2, true, false, (encode_fn_t)encode_ldi},
    {"LDR", 0x6, {TYPE_REG, TYPE_REG, TYPE_NUMBER}, 3, false, false, (encode_fn_t)encode_ldr},
    {"LEA", 0xE, {TYPE_REG, TYPE_LABEL}, 2, true, false, (encode_fn_t)encode_lea},
    {"ST", 0x3, {TYPE_REG, TYPE_LABEL}, 2, true, false, (encode_fn_t)encode_st},
    {"STI", 0xB, {TYPE_REG, TYPE_LABEL}, 2, true, false, (encode_fn_t)encode_sti},

    // Jumps
    {"JSR", 0x4, {TYPE_LABEL}, 1, true, false, (encode_fn_t)encode_jsr},
    {"RET", 0xC, {}, 0, false, false, (encode_fn_t)encode_ret},

    // Traps
    {"TRAP", 0xF, {TYPE_NUMBER}, 1, false, false, (encode_fn_t)encode_trap},
    {"HALT", 0xF, {}, 0, false, false, (encode_fn_t)encode_halt},
    {"GETC", 0xF, {}, 0, false, false, (encode_fn_t)encode_getc},
    {"OUT", 0xF, {}, 0, false, false, (encode_fn_t)encode_out},
    {"PUTS", 0xF, {}, 0, false, false, (encode_fn_t)encode_puts},
    {"IN", 0xF, {}, 0, false, false, (encode_fn_t)encode_in},
    {"PUTSP", 0xF, {}, 0, false, false, (encode_fn_t)encode_putsp},

    // Pseudo-ops
    {".ORIG", 0x0, {TYPE_NUMBER}, 1, false, true, NULL},
    {".FILL", 0x0, {TYPE_NUMBER}, 1, false, true, (encode_fn_t)encode_fill},
    {".BLKW", 0x0, {TYPE_NUMBER}, 1, false, true, (encode_fn_t)encode_blkw},
    {".STRINGZ", 0x0, {TYPE_STRING}, 1, false, true, (encode_fn_t)encode_stringz},
    {".END", 0x0, {}, 0, false, true, (encode_fn_t)encode_end}};

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

segment_t *encode_org(segment_t **ctx_head, token_line_t *tokens, size_t idx)
{
    token_t *ops = tokens->instr[idx].operands;
    uint16_t addr = parse_number(ops[0].value);

    return add_segment(ctx_head, addr);
}

// -------------------------------------------------
// Assembler Driver (public API)
// -------------------------------------------------

/*
 * assemble:
 *   tokens     - tokenized input (symbol table should already be filled by earlier pass)
 *   out_buf    - caller-provided buffer for machine code (words)
 *   out_size   - number of words in out_buf
 * Returns:
 *   number of words emitted into out_buf
 */

segment_t *assemble(token_line_t *tokens)
{
    validate_instruction(tokens);

    segment_t *head = NULL;    // first segment
    segment_t *current = NULL; // segment we're writing to

    for (size_t i = 0; i < tokens->line_count; i++)
    {
        const char *mnemonic = tokens->instr[i].opcode.value;
        instruction_spec_t spec = find_spec(mnemonic);

        if (strcmp(mnemonic, ".ORIG") == 0)
        {
            // special case: ORIG returns new segment
            current = encode_org(&head, tokens, i);

            // If head was NULL, first segment is now current
            if (head == NULL)
                head = current;
        }
        else
        {
            if (!spec.encode_fn)
            {
                report_error(i + 1, "Unknown directive or opcode: %s\n", mnemonic);
                continue;
            }
            if (!current)
            {
                report_error(i + 1, "Code before any .ORIG directive\n");
                continue;
            }
            ((encode_fn_t)spec.encode_fn)(&current, tokens, i);
        }
    }

    return head;
}

segment_t *create_segment(uint16_t origin, size_t capacity)
{
    segment_t *seg = malloc(sizeof(segment_t));
    seg->origin = origin;
    seg->data = malloc(capacity * sizeof(uint16_t));
    seg->size = 0;
    seg->pos = 0;
    seg->next = NULL;
    return seg;
}

segment_t *add_segment(segment_t **ctx, uint16_t origin)
{
    segment_t *seg = create_segment(origin, 128);
    if (!seg)
    {
        fprintf(stderr, "Error: failed to create segment\n");
        return;
    }
    seg->next = NULL;

    if (*ctx == NULL)
    {
        *ctx = seg;
        return seg;
    }
    segment_t *tmp = *ctx;
    while (tmp->next != NULL)
    {
        tmp = tmp->next;
    }
    tmp->next = seg;
    return seg;
}

void load_segments_to_memory(segment_t *ctx, uint16_t *memory)
{
    for (segment_t *seg = ctx; seg != NULL; seg = seg->next)
    {
        for (size_t i = 0; i < seg->pos; i++)
        {
            memory[seg->origin + i] = seg->data[i];
        }
    }
}
