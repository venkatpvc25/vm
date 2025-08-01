#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <pvm/vm.h>
#include <pvm/tokenizer.h>
#include <pvm/utils.h>

// bool read_image(char *file)
// {
//     printf("reading file %s\n", file);
//     return true;
// }

const char *program =
    "; sample LC-3 program\n"
    ".ORIG x3000\n"
    "START   LD   R1, VALUE\n"
    "        ADD  R1, R1, #1\n"
    "        ST   R1, VALUE\n"
    "        HALT\n"
    "VALUE   .FILL #5\n"
    ".END";

int main(int argc, char *argv[])
{
    // TokenLine lines[MAX_LINES];
    // MachineCode bytecode[MAX_LINES];

    // int line_count = tokenize_program(program, lines);
    // print_token_line(lines, line_count);
    // int count = assemble(lines, line_count, bytecode);

    // VM vm;

    // load_program(&vm, bytecode, 7);

    // run(&vm);

    FILE *file = fopen("code.asm", "r");
    char line[256];

    token_array_t tokenLine;

    while (fgets(line, sizeof(line), file))
    {
        trim(line);
        // printf("Line: %s\n", line);
        tokenizer(line, &tokenLine);
    }

    for (int i = 0; i < tokenLine.token_count; i++)
    {
        printf("[%s, %s]\n", token_type_to_string(tokenLine.tokens[i].type), tokenLine.tokens[i].lexeme);
    }

    return 0;
}
