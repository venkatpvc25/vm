// #include <stdio.h>
// #include <stdlib.h>
// #include <stdbool.h>

// #include "pvm/tokenizer.h"
// #include "pvm/assembler.h"
// #include "pvm/vm.h"

// #include <termios.h>
// #include <unistd.h>
// #include <string.h>

// struct termios original_tio;

// void disable_input_buffering()
// {
//     tcgetattr(STDIN_FILENO, &original_tio); // Get current terminal settings
//     struct termios new_tio = original_tio;

//     new_tio.c_lflag &= ~ICANON;                 // Disable canonical mode
//     new_tio.c_lflag &= ~ECHO;                   // Disable echo
//     tcsetattr(STDIN_FILENO, TCSANOW, &new_tio); // Apply new settings
// }

// void restore_input_buffering()
// {
//     tcsetattr(STDIN_FILENO, TCSANOW, &original_tio);
// }

// int main(void)
// {
//     token_line_t lines;
//     size_t line_count = 0;

//     memset(&lines, 0, sizeof(token_line_t));

//     FILE *f = fopen("traps.asm", "r");
//     if (!f)
//     {
//         perror("Failed to open traps.asm");
//         return 1;
//     }

//     tokenize_file(f, &lines, &line_count);
//     fclose(f);

//     uint16_t machine_code[1024];
//     segment_t *seg = assemble(&lines);

//     VM vm;
//     // load_program(&vm, machine_code, bytes_written);
//     load_segments_to_memory(seg, vm.mem);
//     run(&vm);
//     return 0;
// }

#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>

struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;
char *fbp;
long screensize;

int main()
{
    int fbfd = open("/dev/fb0", O_RDWR);
    if (fbfd == -1)
    {
        perror("Error opening fb0");
        return -1;
    }

    ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo);
    ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo);

    screensize = vinfo.yres_virtual * finfo.line_length;
    fbp = (char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);

    return fbfd;
}
