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