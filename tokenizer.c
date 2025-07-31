#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define MAX_TOKENS_PER_LINE 10
#define MAX_LINES 1024

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

typedef struct
{
    TokenType type;
    char lexeme[64];
} Token;

typedef struct
{
    Token tokens[MAX_LINES];
    int token_count;
} TokenLine;

#define MAX_SYMBOLS 100
#define MAX_SYMBOL_LENGTH 64

char symbols[MAX_SYMBOLS][MAX_SYMBOL_LENGTH];
static int symbol_count = 0;

static int token_count = 0;

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

void tokenizer(const char *line, TokenLine *tokens)
{

    if (is_comment(line))
        return;

    char *word = read_token(&line);

    if (is_directive(word))
    {
        Token *token = &(tokens->tokens[token_count++]);
        token->type = TOKEN_DIRECTIVE;
        strcpy(token->lexeme, word);
        free(word);
        word = read_token(&line);
        token = &(tokens->tokens[token_count++]);
        if (is_hex(word))
            token->type = TOKEN_HEX;
        else if (is_const(word))
            token->type = TOKEN_NUMBER;
        else if (is_string(word))
            token->type = TOKEN_STRING;
        else
        {
            token->type = TOKEN_LABEL;
            strcpy(symbols[symbol_count], word);
            symbol_count++;
        }
        strcpy(token->lexeme, word);
        return;
    }

    if (is_instruction(word))
    {
        Token *token = &(tokens->tokens[token_count++]);
        token->type = TOKEN_OPCODE;
        strcpy(token->lexeme, word);
        word = NULL;
        while ((word = read_token(&line)) != NULL)
        {
            if (is_comment(word))
                continue;
            Token *token = &(tokens->tokens[token_count++]);
            token->type = is_comma_separator(word) ? TOKEN_COMMA : TOKEN_REGISTER;
            strcpy(token->lexeme, word);
            word = NULL;
        }
    }

    printf("line: %s\n", line);
}

int main()
{
    FILE *file = fopen("code.asm", "r");
    char line[256];

    TokenLine tokenLine;

    while (fgets(line, sizeof(line), file))
    {
        trim(line);
        printf("Line: %s\n", line);
        tokenizer(line, &tokenLine);
    }
}