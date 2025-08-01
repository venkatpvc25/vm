#ifndef VM_SYMBOL_H
#define VM_SYMBOL_H

#include <stdint.h>
#include <stdbool.h>

#include "tokenizer.h"

typedef struct symbol_entry
{
    char label[32];
    uint16_t address;
} symbol_entry_t;

void add_symbol(token_array_t *tokens, const char *symbol, uint16_t address);
symbol_entry_t *look_symbol(token_array_t *tokens, const char *symbol);
bool exists_symbol(token_array_t *tokens, const char *symbol);

#endif