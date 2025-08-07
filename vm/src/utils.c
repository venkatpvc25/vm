#include <stdlib.h>
#include <stdint.h>

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

int16_t parse_number(const char *str)
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
        return value = strtol(str, NULL, 10);
    }

    return (uint16_t)value;
}

int16_t sign_extend(uint16_t x, int bit_count)
{
    if ((x >> (bit_count - 1)) & 1)
        x |= (0xFFFF << bit_count);
    return x;
}