#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "pvm/tokenizer.h"
#include "pvm/assembler.h"

#include <termios.h>
#include <unistd.h>

struct termios original_tio;

void disable_input_buffering()
{
    tcgetattr(STDIN_FILENO, &original_tio); // Get current terminal settings
    struct termios new_tio = original_tio;

    new_tio.c_lflag &= ~ICANON;                 // Disable canonical mode
    new_tio.c_lflag &= ~ECHO;                   // Disable echo
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio); // Apply new settings
}

void restore_input_buffering()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &original_tio);
}

int main()
{
    token_line_t lines;
    size_t line_count = 0;

    FILE *f = fopen("traps.asm", "r");
    tokenize_file(f, &lines, &line_count);

    // // First pass: label collection
    // for (size_t i = 0; i < line_count; i++)
    // {
    //     collect_labels(&lines[i]);
    // }

    // Second pass: encoding
    for (size_t i = 0; i < line_count; i++)
    {
        encode_line(&lines.instr);
    }

    // emit_to_file("build/output.obj");
}
