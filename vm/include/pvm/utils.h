#ifndef VM_UTILS_H
#define VM_UTILS_H ;

#include <stdint.h>

void trim(char *str);
void skip_whitespace(char **str);
int16_t parse_number(const char *str);

int16_t sign_extend(uint16_t x, int bit_count);

#endif