#ifndef VM_SYMBOL_H
#define VM_SYMBOL_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct symbol_entry
{
    char label[32];
    uint16_t address;
} symbol_entry_t;

typedef struct token_line token_line_t;

void add_symbol(token_line_t *tokens, const char *symbol, uint16_t address);
symbol_entry_t *look_symbol(token_line_t *tokens, const char *symbol);
bool exists_symbol(token_line_t *tokens, const char *symbol);
void print_symbol(symbol_entry_t **symbols, size_t count);

#endif