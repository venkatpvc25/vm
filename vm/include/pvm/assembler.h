#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <stdint.h>
#include <stdlib.h>

#include "tokenizer.h"

#define MAX_MACHINE_CODE 1024

#define MAX_TOKENS 6

typedef struct segment
{
    uint16_t origin;      // absolute start address
    uint16_t *data;       // segment contents
    size_t size;          // total size allocated (words)
    size_t pos;           // current write index
    struct segment *next; // linked list pointer
} segment_t;

typedef void (*encode_fn_t)(segment_t **ctx, token_line_t *tokens, size_t idx);

typedef struct
{
    const char *mnemonic;
    uint8_t opcode;
    operand_type operand_types[MAX_TOKENS];
    int operand_count;
    bool supports_immediate;
    bool is_pseudo;
    encode_fn_t encode_fn;
} instruction_spec_t;

segment_t *assemble(token_line_t *tokens);
instruction_spec_t find_spec(const char *mnemonic);
segment_t *create_segment(uint16_t origin, size_t capacity);
segment_t *add_segment(segment_t **ctx, uint16_t origin);
void load_segments_to_memory(segment_t *ctx, uint16_t *memory);
#endif
