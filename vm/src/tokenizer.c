#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

#include "pvm/tokenizer.h"
#include "pvm/utils.h"
#include "pvm/symbol.h"

#define MAX_TOKENS_PER_LINE 10
#define MAX_LINES 1024

token_line_t token_lines;

int line_number = 0;
int token_count = 0;
size_t orig = 0;
static uint16_t current_address = 0;

const char *lc3_opcodes[] = {
    "ADD", "AND",
    "BR", "BRn", "BRz", "BRp", "BRnz", "BRzp", "BRnp", "BRnzp",
    "JMP", "JSR", "JSRR",
    "LD", "LDI", "LDR", "LEA",
    "NOT", "RET", "RTI",
    "ST", "STI", "STR",
    "TRAP"};

const int lc3_opcode_count = sizeof(lc3_opcodes) / sizeof(lc3_opcodes[0]);

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
    case TOKEN_UNKNOWN:
        return "UNKNOWN";
    default:
        return "INVALID";
    }
}

static inline bool is_number(const char *str) { return str[0] == '#' || str[0] == 'x'; }
static inline bool is_string(const char *str) { return str[0] == '"'; }
static inline bool is_hex(const char *str) { return str[0] == 'x' || str[0] == 'X'; }
static inline bool is_const(const char *str) { return str[0] == '#'; }
static inline bool is_register(const char *cmnt) { return cmnt[0] == 'R' || cmnt[0] == 'r'; }
static inline bool is_directive(const char *cmnt) { return cmnt[0] == '.'; }
static inline bool is_comment(const char *cmnt) { return cmnt[0] == ';'; }
static inline bool is_comma_separator(const char *cmnt) { return cmnt[0] == ','; }
static inline bool is_immediate(const char *str) { return str[0] == 'x' || str[0] == 'X' || str[0] == '#'; }

static inline bool is_instruction(const char *instr)
{
    for (int i = 0; i < lc3_opcode_count; i++)
    {
        if (strcmp(instr, lc3_opcodes[i]) == 0)
        {
            return true;
        }
    }
    return false;
}

static inline bool is_label(const char *str)
{
    if (!is_instruction(str) || !is_comment(str))
        return true;
    return false;
}

char *read_token(char **str)
{
    skip_whitespace(str);

    if (**str == '\0' || **str == ';')
        return NULL;

    char *start = *str;
    int len = 0;

    while (**str != '\0' && **str != ';' && **str != ',' && !isspace(**str))
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
    if (!token)
        return NULL;

    strncpy(token, start, len);
    token[len] = '\0';

    skip_whitespace(str);
    if (**str == ',')
    {
        (*str)++;
    }

    return token;
}

void parse_opcode(const char *word, const char **line)
{
    instr_t *current = &(token_lines.instr[token_lines.line_count++]);
    current->line_number = token_lines.line_count - 1;
    token_t *opcode = &(current->opcode);
    opcode->type = TOKEN_OPCODE;
    strcpy(opcode->value, word);
    free(word);
    word = NULL;
    while ((word = read_token(line)) != NULL)
    {
        if (is_comment(word))
            continue;

        token_t *operand = &(current->operands[current->operand_count++]);
        operand->type = TOKEN_UNKNOWN;
        operand->operand_type = TYPE_NONE;

        if (is_comma_separator(word))
        {
            operand->type = TOKEN_COMMA;
            operand->operand_type = TYPE_NONE;
        }
        else if (is_register(word))
        {
            operand->type = TOKEN_REGISTER;
            operand->operand_type = TYPE_REG;
        }
        else if (is_immediate(word))
        {
            operand->type = TOKEN_IMMEDIATE;
            operand->operand_type = TYPE_IMM5;
        }
        else if (is_string(word))
        {
            operand->type = TOKEN_STRING;
            operand->operand_type = TYPE_STRING;
        }
        else if (is_hex(word) || is_number(word))
        {
            operand->type = TOKEN_NUMBER;
            operand->operand_type = TYPE_NUMBER;
        }
        else
        {
            operand->type = TOKEN_IDENTIFIER;
            operand->operand_type = TYPE_LABEL;
        }

        strcpy(operand->value, word);
        free(word);
        word = NULL;
    }
}

void parse_directive(const char *word, const char **line)
{
    instr_t *current = &(token_lines.instr[token_lines.line_count++]);
    current->line_number = token_lines.line_count - 1;
    token_t *opcode = &(current->opcode);
    opcode->type = TOKEN_DIRECTIVE;
    strcpy(opcode->value, word);
    free(word);
    word = NULL;
    while ((word = read_token(line)) != NULL)
    {
        if (is_comment(word))
            continue;
        if (strcmp(opcode->value, ".ORIG") == 0)
        {
            orig = parse_number(word);
        }

        token_t *operand = &(current->operands[current->operand_count++]);
        operand->type = TOKEN_UNKNOWN;
        operand->operand_type = TYPE_NONE;
        if (is_immediate(word))
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
            operand->type = TOKEN_LABEL;
            operand->operand_type = TYPE_LABEL;
        }
        strcpy(operand->value, word);
        free(word);
        word = NULL;
    }
}

void tokenizer(const char *line)
{
    if (is_comment(line))
        return;

    char *word = NULL;
    while ((word = read_token(&line)) != NULL) // <-- this is key
    {
        if (is_directive(word))
        {
            parse_directive(word, &line);
            continue;
        }
        if (is_instruction(word))
        {
            parse_opcode(word, &line);
            continue;
        }

        if (!exists_symbol(&token_lines, word))
        {
            uint16_t address = orig + token_lines.line_count;
            add_symbol(&token_lines, word, address);
        }
    }
}

#include <stdio.h>

void print_token(const token_t *token)
{
    if (token == NULL)
    {
        printf("NULL");
        return;
    }
    printf("{ type: %s, lexeme: \"%s\" }", token_type_to_string(token->type), token->value);
}

void print_token_line()
{
    for (int n = 0; n < token_lines.line_count; n++)
    {
        instr_t instr = token_lines.instr[n];
        printf("Opcode: ");
        print_token(&instr.opcode);
        printf("\n");

        for (int i = 0; i < instr.operand_count; i++)
        {
            printf("  [%d] ", i);
            print_token(&instr.operands[i]);
            printf("\n");
        }
    }
    print_symbol(token_lines.symbols, token_lines.symbol_count);
}

// void tokenizer(const char *line, token_array_t *tokens)
// {

//     if (is_comment(line))
//         return;

//     char *word = NULL;
//     while ((word = read_token(&line)) != NULL) // <-- this is key
//     {
//         if (is_directive(word))
//         {
//             parse_directive(word, &line, tokens);
//             continue;
//         }
//         if (is_instruction(word))
//         {
//             parse_opcode(word, &line, tokens);
//             continue;
//         }

//         token_t *token = &(tokens->tokens[token_count++]);
//         token->type = TOKEN_LABEL;
//         strcpy(token->lexeme, word);
//         if (!exists_symbol(tokens, word))
//         {
//             uint16_t address = orig + current_address;
//             add_symbol(tokens, word, address);
//         }
//     }
//     current_address++;
//     tokens->token_count = token_count;
// }

// int main()
// {
//     FILE *file = fopen("code.asm", "r");
//     char line[256];

//     token_array_t tokenLine;

//     while (fgets(line, sizeof(line), file))
//     {
//         trim(line);
//         // printf("Line: %s\n", line);
//         tokenizer(line, &tokenLine);
//     }

//     for (int i = 0; i < tokenLine.token_count; i++)
//     {
//         printf("[%s, %s]\n", token_type_to_string(tokenLine.tokens[i].type), tokenLine.tokens[i].lexeme);
//     }

//     return 0;
// }