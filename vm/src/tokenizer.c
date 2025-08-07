#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

#include "pvm/tokenizer.h"
#include "pvm/utils.h"
#include "pvm/symbol.h"

#define MAX_TOKENS_PER_LINE 10

static uint16_t orig = 0;

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

static const char *lc3_opcodes[] = {
    "ADD", "AND",
    "BR", "BRn", "BRz", "BRp", "BRnz", "BRzp", "BRnp", "BRnzp",
    "JMP", "JSR", "JSRR",
    "LD", "LDI", "LDR", "LEA",
    "NOT", "RET", "RTI",
    "ST", "STI", "STR",
    "TRAP"};

static const char *lc3_traps[] = {
    "GETC", "OUT", "PUTS", "IN", "PUTSP", "HALT"};

static bool is_number(const char *str) { return str[0] == '#' || str[0] == 'x' || str[0] == 'X'; }
static bool is_string(const char *str) { return str[0] == '"'; }
static bool is_register(const char *s) { return s[0] == 'R' || s[0] == 'r'; }
static bool is_directive(const char *s) { return s[0] == '.'; }
static bool is_comment(const char *s) { return s[0] == ';'; }
static bool is_comma(const char *s) { return s[0] == ','; }

bool is_instruction(const char *word)
{
    for (int i = 0; i < sizeof(lc3_opcodes) / sizeof(lc3_opcodes[0]); i++)
        if (strcmp(word, lc3_opcodes[i]) == 0)
            return true;
    return false;
}

static bool is_trap(const char *word)
{
    for (int i = 0; i < sizeof(lc3_traps) / sizeof(lc3_traps[0]); i++)
        if (strcmp(word, lc3_traps[i]) == 0)
            return true;
    return false;
}

// -----------------------------------------------------------------------------
// Token Reader
// -----------------------------------------------------------------------------

static char *read_token(char **str)
{
    skip_whitespace(str);
    if (**str == '\0' || **str == ';')
        return NULL;

    char *start = *str;
    int len = 0;

    while (**str && !isspace(**str) && **str != ',' && **str != ';')
    {
        (*str)++;
        len++;
    }

    if (len == 0)
    {
        if (**str == ',')
        {
            (*str)++;
            skip_whitespace(str);
            return read_token(str);
        }
        return NULL;
    }

    char *token = malloc(len + 1);
    strncpy(token, start, len);
    token[len] = '\0';

    skip_whitespace(str);
    if (**str == ',')
        (*str)++;

    return token;
}

// -----------------------------------------------------------------------------
// Parse Lines
// -----------------------------------------------------------------------------

static void parse_operands(instr_t *instr, const char **line)
{
    char *word;
    while ((word = read_token((char **)line)) != NULL)
    {
        if (is_comment(word))
        {
            free(word);
            break;
        }

        token_t *operand = &instr->operands[instr->operand_count++];
        operand->type = TOKEN_UNKNOWN;
        operand->operand_type = TYPE_NONE;

        if (is_comma(word))
        {
            operand->type = TOKEN_COMMA;
        }
        else if (is_register(word))
        {
            operand->type = TOKEN_REGISTER;
            operand->operand_type = TYPE_REG;
        }
        else if (is_number(word))
        {
            operand->type = TOKEN_NUMBER;
            operand->operand_type = TYPE_NUMBER;
        }
        else if (is_string(word))
        {
            operand->type = TOKEN_STRING;
            operand->operand_type = TYPE_STRING;
        }
        else
        {
            operand->type = TOKEN_IDENTIFIER;
            operand->operand_type = TYPE_LABEL;
        }

        strncpy(operand->value, word, sizeof(operand->value));
        free(word);
    }
}

static void parse_instruction(const char *word, const char **line, token_line_t *tokens, token_type type)
{
    instr_t *instr = &tokens->instr[tokens->line_count++];
    instr->line_number = tokens->line_count - 1;

    instr->opcode.type = type;
    strncpy(instr->opcode.value, word, sizeof(instr->opcode.value));
    free((void *)word);

    parse_operands(instr, line);
}

// -----------------------------------------------------------------------------
// Tokenizer
// -----------------------------------------------------------------------------

token_line_t *tokenizer(const char *line, token_line_t *tokens)
{
    if (is_comment(line))
        return NULL;

    char *word = NULL;
    while ((word = read_token((char **)&line)) != NULL)
    {
        if (is_comment(word))
        {
            free(word);
            break;
        }

        if (is_directive(word))
        {
            if (strcmp(word, ".ORIG") == 0)
            {
                char *orig_val = read_token((char **)&line);
                if (orig_val)
                {
                    orig = parse_number(orig_val);
                    free(orig_val);
                }
            }
            parse_instruction(word, &line, tokens, TOKEN_DIRECTIVE);
        }
        else if (is_instruction(word))
        {
            parse_instruction(word, &line, tokens, TOKEN_OPCODE);
        }
        else if (is_trap(word))
        {
            parse_instruction(word, &line, tokens, TOKEN_TRAP);
        }
        else
        {
            // Assume label
            if (!exists_symbol(tokens, word))
            {
                uint16_t address = orig + tokens->line_count;
                add_symbol(tokens, word, address);
            }
            free(word);
        }
    }

    return tokens;
}

// -----------------------------------------------------------------------------
// Debug Printers
// -----------------------------------------------------------------------------

const char *token_type_to_string(token_type type)
{
    switch (type)
    {
    case TOKEN_LABEL:
        return "LABEL";
    case TOKEN_OPCODE:
        return "OPCODE";
    case TOKEN_REGISTER:
        return "REGISTER";
    case TOKEN_IMMEDIATE:
        return "IMMEDIATE";
    case TOKEN_DIRECTIVE:
        return "DIRECTIVE";
    case TOKEN_STRING:
        return "STRING";
    case TOKEN_COMMENT:
        return "COMMENT";
    case TOKEN_IDENTIFIER:
        return "IDENTIFIER";
    case TOKEN_HEX:
        return "HEX";
    case TOKEN_NUMBER:
        return "NUMBER";
    case TOKEN_COMMA:
        return "COMMA";
    case TOKEN_TRAP:
        return "TRAP";
    default:
        return "UNKNOWN";
    }
}

void print_token(const token_t *token)
{
    if (!token)
        return;
    printf("{ type: %s, value: \"%s\" }", token_type_to_string(token->type), token->value);
}

void print_token_line(token_line_t tokens)
{
    for (int i = 0; i < tokens.line_count; i++)
    {
        instr_t instr = tokens.instr[i];
        printf("Line %d: ", instr.line_number);
        print_token(&instr.opcode);
        printf("\n");

        for (int j = 0; j < instr.operand_count; j++)
        {
            printf("  Operand %d: ", j);
            print_token(&instr.operands[j]);
            printf("\n");
        }
    }

    print_symbol(tokens.symbols, tokens.symbol_count);
}

void tokenize_file(FILE *file, token_line_t *lines, size_t *line_count)
{
    char buffer[256];
    int line_num = 1;

    while (fgets(buffer, sizeof(buffer), file))
    {
        trim(buffer);
        tokenizer(buffer, &lines[*line_count]);
        lines[*line_count].line_count = line_num++;
        (*line_count)++;
    }
}
