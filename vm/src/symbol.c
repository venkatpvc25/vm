#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "pvm/tokenizer.h"

#include "pvm/symbol.h"

void add_symbol(token_line_t *tokens, const char *symbol, uint16_t address)
{
    if (tokens->symbol_count >= MAX_SYMBOLS)
    {
        return;
    }

    symbol_entry_t *entry = malloc(sizeof(symbol_entry_t));
    if (!entry)
    {
        return;
    }

    strncpy(entry->label, symbol, sizeof(entry->label) - 1);
    entry->label[sizeof(entry->label) - 1] = '\0';
    entry->address = address;

    tokens->symbols[tokens->symbol_count++] = entry;
}

symbol_entry_t *look_symbol(token_line_t *tokens, const char *symbol)
{
    symbol_entry_t *INVALID_SYMBOL = malloc(sizeof(symbol_entry_t));
    memset(INVALID_SYMBOL->label, 0, sizeof(INVALID_SYMBOL->label));
    INVALID_SYMBOL->address = -1;

    for (int i = 0; i < tokens->symbol_count; i++)
    {
        if (strcmp(tokens->symbols[i]->label, symbol))
        {
            return &tokens->symbols[i];
        }
    }
    return INVALID_SYMBOL;
}

bool exists_symbol(token_line_t *tokens, const char *symbol)
{
    if (tokens->symbol_count == 0)
        return false;
    for (int i = 0; i < tokens->symbol_count; i++)
    {
        if (strcmp(tokens->symbols[i]->label, symbol))
        {
            return true;
        }
    }
    return false;
}

void print_symbol(symbol_entry_t **symbols, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        symbol_entry_t *symbol = symbols[i];
        if (symbol != NULL)
        {
            printf("  [%zu] label: %s, address: 0x%04X\n", i, symbol->label, symbol->address);
        }
        else
        {
            printf("  [%zu] NULL symbol entry\n", i);
        }
    }
}