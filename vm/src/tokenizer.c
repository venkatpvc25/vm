#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

#include "pvm/tokenizer.h"
#include "pvm/utils.h"

#define MAX_TOKENS_PER_LINE 10
#define MAX_LINES 1024

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
    if (**str == ',')
    {

        char *result = malloc(sizeof(char));
        if (!result)
            return NULL;

        strncpy(result, *str, 1);
        result[1] = '\0';
        (*str)++;
        return result;
    }
    char *start = *str;
    int str_len = 0;

    while (**str != '\0' && **str != ';' && **str != ',' && **str != ' ')
    {
        str_len++;
        (*str)++;
    }

    char *result = malloc(str_len + 1);
    if (!result)
        return NULL;

    strncpy(result, start, str_len);
    result[str_len] = '\0';

    return result;
}

void parse_opcode(const char *opcode, const char **line, token_array_t *tokens)
{
    token_t *token = &(tokens->tokens[token_count++]);
    token->type = TOKEN_OPCODE;
    strcpy(token->lexeme, opcode);
    char *word = NULL;
    while ((word = read_token(line)) != NULL)
    {
        if (is_comment(word))
            continue;
        token_t *token = &(tokens->tokens[token_count++]);
        if (is_hex(word))
            token->type = TOKEN_HEX;
        else if (is_const(word))
            token->type = TOKEN_NUMBER;
        else if (is_string(word))
            token->type = TOKEN_STRING;
        else if (is_comma_separator(word))
            token->type = TOKEN_COMMA;
        else if (is_register(word))
            token->type = TOKEN_REGISTER;
        else
            token->type = TOKEN_LABEL;
        strcpy(token->lexeme, word);
        free(word);
        word = NULL;
    }
}

void parse_directive(const char *opcode, const char **line, token_array_t *tokens)
{
    token_t *token = &(tokens->tokens[token_count++]);
    token->type = TOKEN_DIRECTIVE;
    strcpy(token->lexeme, opcode);
    char *word = NULL;
    while ((word = read_token(line)) != NULL)
    {
        if (is_comment(word))
            continue;
        if (strcmp(tokens->tokens[token_count - 1].lexeme, ".ORIG") == 0)
        {
            orig = parse_number(word);
        }

        token_t *token = &(tokens->tokens[token_count++]);
        if (is_hex(word))
            token->type = TOKEN_HEX;
        else if (is_const(word))
            token->type = TOKEN_NUMBER;
        else if (is_string(word))
            token->type = TOKEN_STRING;
        else
        {
            token->type = TOKEN_LABEL;
        }
        strcpy(token->lexeme, word);
        free(word);
        word = NULL;
    }
}

void tokenizer(const char *line, token_array_t *tokens)
{

    if (is_comment(line))
        return;

    char *word = NULL;
    while ((word = read_token(&line)) != NULL) // <-- this is key
    {
        if (is_directive(word))
        {
            parse_directive(word, &line, tokens);
            continue;
        }
        if (is_instruction(word))
        {
            parse_opcode(word, &line, tokens);
            continue;
        }

        token_t *token = &(tokens->tokens[token_count++]);
        token->type = TOKEN_LABEL;
        strcpy(token->lexeme, word);
        if (!exists_symbol(tokens, word))
        {
            uint16_t address = orig + current_address;
            add_symbol(tokens, word, address);
        }
    }
    current_address++;
    tokens->token_count = token_count;
}

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