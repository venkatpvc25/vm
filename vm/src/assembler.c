#include "assembler.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static int reg_index(const char *reg)
{
    for (int i = 0; i < 8; i++)
    {
        char name[4];
        sprintf(name, "R%d", i);
        if (strcmp(reg, name) == 0)
            return i;
    }
    return -1;
}

typedef struct
{
    char label[64];
    uint16_t address;
} Label;

static Label symbol_table[MAX_LABELS];
static int symbol_count = 0;

static void add_label(const char *label, uint16_t addr)
{
    strcpy(symbol_table[symbol_count].label, label);
    symbol_table[symbol_count].address = addr;
    symbol_count++;
}

static int lookup_label(const char *label)
{
    for (int i = 0; i < symbol_count; i++)
    {
        if (strcmp(symbol_table[i].label, label) == 0)
        {
            return symbol_table[i].address;
        }
    }
    fprintf(stderr, "Error: Unknown label %s\n", label);
    exit(1);
}

static int parse_immediate(const char *lexeme, uint16_t pc)
{
    if (lexeme[0] == '#')
        return atoi(&lexeme[1]);
    if (lexeme[0] == 'x' || lexeme[0] == 'X')
        return (int)strtol(&lexeme[1], NULL, 16);
    return lookup_label(lexeme) - pc - 1; // PC-relative offset
}

int assemble(TokenLine *lines, int line_count, MachineCode *output)
{
    uint16_t pc = 0x3000;
    int out_count = 0;

    // First pass: collect labels
    for (int i = 0; i < line_count; ++i)
    {
        TokenLine *tl = &lines[i];
        if (tl->token_count == 0)
            continue;

        if (tl->tokens[0].type == TOKEN_LABEL)
        {
            add_label(tl->tokens[0].lexeme, pc);
            // Shift tokens left
            for (int j = 0; j < tl->token_count - 1; j++)
                tl->tokens[j] = tl->tokens[j + 1];
            tl->token_count--;
        }

        Token *op = &tl->tokens[0];
        if (op->type == TOKEN_DIRECTIVE)
        {
            if (strcmp(op->lexeme, ".ORIG") == 0)
            {
                pc = parse_immediate(tl->tokens[1].lexeme, 0);
            }
            else if (strcmp(op->lexeme, ".FILL") == 0 || strcmp(op->lexeme, ".STRINGZ") == 0)
            {
                pc++;
            }
            else if (strcmp(op->lexeme, ".BLKW") == 0)
            {
                pc += parse_immediate(tl->tokens[1].lexeme, 0);
            }
        }
        else if (op->type == TOKEN_OPCODE)
        {
            pc++;
        }
    }

    // Second pass: generate code
    pc = 0x3000;
    for (int i = 0; i < line_count; ++i)
    {
        TokenLine *tl = &lines[i];
        if (tl->token_count == 0)
            continue;

        Token *op = &tl->tokens[0];
        uint16_t instr = 0;

        if (op->type == TOKEN_DIRECTIVE)
        {
            if (strcmp(op->lexeme, ".ORIG") == 0)
            {
                pc = parse_immediate(tl->tokens[1].lexeme, 0);
            }
            else if (strcmp(op->lexeme, ".FILL") == 0)
            {
                instr = parse_immediate(tl->tokens[1].lexeme, pc);
                output[out_count++] = (MachineCode){pc++, instr};
            }
            else if (strcmp(op->lexeme, ".BLKW") == 0)
            {
                int count = parse_immediate(tl->tokens[1].lexeme, pc);
                for (int j = 0; j < count; j++)
                    output[out_count++] = (MachineCode){pc++, 0};
            }
            else if (strcmp(op->lexeme, ".STRINGZ") == 0)
            {
                const char *str = tl->tokens[1].lexeme + 1;
                for (int j = 0; str[j] && str[j] != '"'; j++)
                    output[out_count++] = (MachineCode){pc++, str[j]};
                output[out_count++] = (MachineCode){pc++, 0};
            }
            continue;
        }

        if (op->type != TOKEN_OPCODE)
            continue;

        const char *opcode = op->lexeme;

        if (strcmp(opcode, "ADD") == 0 || strcmp(opcode, "AND") == 0)
        {
            instr |= strcmp(opcode, "ADD") == 0 ? 0x1000 : 0x5000;
            instr |= reg_index(tl->tokens[1].lexeme) << 9;
            instr |= reg_index(tl->tokens[2].lexeme) << 6;
            if (tl->tokens[3].type == TOKEN_REGISTER)
                instr |= reg_index(tl->tokens[3].lexeme);
            else
                instr |= 0x20 | (parse_immediate(tl->tokens[3].lexeme, pc) & 0x1F);
        }
        else if (strcmp(opcode, "NOT") == 0)
        {
            instr |= 0x903F;
            instr |= reg_index(tl->tokens[1].lexeme) << 9;
            instr |= reg_index(tl->tokens[2].lexeme) << 6;
        }
        else if (strcmp(opcode, "LD") == 0)
        {
            instr |= 0x2000;
            instr |= reg_index(tl->tokens[1].lexeme) << 9;
            instr |= (parse_immediate(tl->tokens[2].lexeme, pc) & 0x1FF);
        }
        else if (strcmp(opcode, "LEA") == 0)
        {
            instr |= 0xE000;
            instr |= reg_index(tl->tokens[1].lexeme) << 9;
            instr |= (parse_immediate(tl->tokens[2].lexeme, pc) & 0x1FF);
        }
        else if (strcmp(opcode, "BR") == 0 || strncmp(opcode, "BR", 2) == 0)
        {
            instr |= 0x0000;
            if (strchr(opcode, 'n'))
                instr |= 0x0800;
            if (strchr(opcode, 'z'))
                instr |= 0x0400;
            if (strchr(opcode, 'p'))
                instr |= 0x0200;
            instr |= (parse_immediate(tl->tokens[1].lexeme, pc) & 0x1FF);
        }

        output[out_count++] = (MachineCode){pc++, instr};
    }

    return out_count;
}