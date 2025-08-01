#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

#define MAX_TOKENS_PER_LINE 10
#define MAX_LINES 1024
#define MAX_SYMBOLS 64
#define MAX_SYMBOLS 100
#define MAX_SYMBOL_LENGTH 64

char symbols[MAX_SYMBOLS][MAX_SYMBOL_LENGTH];
int symbol_count = 0;
int token_count = 0;
size_t orig = 0;
uint16_t current_address = 0;

typedef enum
{
    TOKEN_LABEL,
    TOKEN_OPCODE,
    TOKEN_REGISTER,
    TOKEN_IMMEDIATE,
    TOKEN_DIRECTIVE,
    TOKEN_STRING,
    TOKEN_COMMENT,
    TOKEN_IDENTIFIER,
    TOKEN_HEX,
    TOKEN_NUMBER,
    TOKEN_COMMA,
    TOKEN_UNKNOWN
} TokenType;

typedef struct Symbol
{
    char label[32];
    uint16_t address;
} SymbolEntry;

typedef struct
{
    TokenType type;
    char lexeme[64];
} Token;

typedef struct
{
    Token tokens[MAX_LINES];
    SymbolEntry symbols[MAX_SYMBOLS];
    int symbol_count;
    int token_count;
} TokenLine;

void add_symbol(TokenLine *tokens, const char *symbol, uint16_t address)
{
    tokens->symbols[tokens->symbol_count++] = (SymbolEntry){.label = symbol, .address = address};
}

SymbolEntry look_symbol(TokenLine *tokens, const char *symbol)
{
    const SymbolEntry INVALID_SYMBOL = {.label = "", .address = -1};

    for (int i = 0; i < symbol_count; i++)
    {
        if (strcmp(tokens->symbols[i].label, symbol))
        {
            return tokens->symbols[i];
        }
    }
    return INVALID_SYMBOL;
}

bool exists_symbol(TokenLine *tokens, const char *symbol)
{
    if (symbol_count == 0)
        return false;
    for (int i = 0; i < symbol_count; i++)
    {
        if (strcmp(tokens->symbols[i].label, symbol))
        {
            return true;
        }
    }
    return false;
}

const char *lc3_opcodes[] = {
    "ADD", "AND",
    "BR", "BRn", "BRz", "BRp", "BRnz", "BRzp", "BRnp", "BRnzp",
    "JMP", "JSR", "JSRR",
    "LD", "LDI", "LDR", "LEA",
    "NOT", "RET", "RTI",
    "ST", "STI", "STR",
    "TRAP"};

int tokenize_program(const char *program_text, TokenLine *lines);

void print_token(const Token *t);
void print_token_line(const TokenLine *lines, int line_count);

const int lc3_opcode_count = sizeof(lc3_opcodes) / sizeof(lc3_opcodes[0]);

const char *token_type_to_string(TokenType type)
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

static inline bool is_register(const char *cmnt)
{
    return cmnt[0] == 'R' || cmnt[0] == 'r';
}

static inline bool is_directive(const char *cmnt)
{
    return cmnt[0] == '.';
}

static inline bool is_comment(const char *cmnt) { return cmnt[0] == ';'; }
static inline bool is_comma_separator(const char *cmnt) { return cmnt[0] == ','; }

static inline bool is_label(const char *str)
{
    if (!is_instruction(str) || !is_comment(str))
        return true;
    return false;
}

static inline bool is_number(const char *str) { return str[0] == '#' || str[0] == 'x'; }

static inline bool is_string(const char *str) { return str[0] == '"'; }

static inline bool is_hex(const char *str) { return str[0] == 'x' || str[0] == 'X'; }
static inline bool is_const(const char *str) { return str[0] == '#'; }

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

void trim(char *str)
{
    if (str == NULL)
        return;

    size_t len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1]))
    {
        str[--len] = '\0';
    }

    char *start = str;
    while (isspace((unsigned char)*start))
    {
        start++;
    }

    if (start != str)
    {
        memmove(str, start, strlen(start) + 1);
    }
}

void skip_whitespace(char **str)
{
    while (isspace(**str))
    {
        (*str)++;
    }
}

uint16_t parse_number(const char *str)
{
    long value = 0;

    if (str[0] == '#')
    {
        value = strtol(str + 1, NULL, 10);
    }
    else if (str[0] == 'x' || str[0] == 'X')
    {
        value = strtol(str + 1, NULL, 16);
    }
    else
    {
        return 0;
    }

    return (uint16_t)value;
}

void parse_opcode(const char *opcode, const char **line, TokenLine *tokens)
{
    Token *token = &(tokens->tokens[token_count++]);
    token->type = TOKEN_OPCODE;
    strcpy(token->lexeme, opcode);
    char *word = NULL;
    while ((word = read_token(line)) != NULL)
    {
        if (is_comment(word))
            continue;
        Token *token = &(tokens->tokens[token_count++]);
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

void parse_directive(const char *opcode, const char **line, TokenLine *tokens)
{
    Token *token = &(tokens->tokens[token_count++]);
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

        Token *token = &(tokens->tokens[token_count++]);
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

void tokenizer(const char *line, TokenLine *tokens)
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

        Token *token = &(tokens->tokens[token_count++]);
        token->type = TOKEN_LABEL;
        strcpy(token->lexeme, word);
        if (!symbol_exists(tokens, word))
        {
            uint16_t address = orig + current_address;
            add_symbol(tokens, word, address);
        }
    }
    current_address++;
    tokens->token_count = token_count;
}

int main()
{
    FILE *file = fopen("code.asm", "r");
    char line[256];

    TokenLine tokenLine;

    while (fgets(line, sizeof(line), file))
    {
        trim(line);
        // printf("Line: %s\n", line);
        tokenizer(line, &tokenLine);
    }

    for (int i = 0; i < tokenLine.token_count; i++)
    {
        printf("[%s, %s]\n", token_type_to_string(tokenLine.tokens[i].type), tokenLine.tokens[i].lexeme);
    }

    return 0;
}