#include <stdio.h>
#include <stdarg.h>

#include "pvm/error.h"

void report_error(int line, const char *fmt, ...)
{
    fprintf(stderr, "Error on line %d: ", line);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}
