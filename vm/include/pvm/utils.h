#ifndef VM_UTILS_H
#define VM_UTILS_H ;

#include <stdint.h>

void trim(char *str);
void skip_whitespace(char **str);
uint16_t parse_number(const char *str);

#endif