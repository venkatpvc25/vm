
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <stdio.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

#include "pvm/vm.h"
#include "pvm/utils.h"

static struct termios original_tio;

#define ADD_MODE_BIT 5
#define JSR_MODE_BIT 11

#define KBSR 0xFE00
#define KBDR 0xFE02
#define DSR 0xFE04
#define DDR 0xFE06
#define MCR 0xFFFE

static inline uint16_t get_bit_at_position(uint16_t value, uint16_t position)
{
    return (value >> position) & 1;
}

typedef enum
{
    FL_POS = 1 << 0, /* P */
    FL_ZRO = 1 << 1, /* Z */
    FL_NEG = 1 << 2, /* N */
} ConditionFlags;

typedef enum
{
    OP_BR = 0,
    OP_ADD,
    OP_LD,
    OP_ST,
    OP_JSR,
    OP_AND,
    OP_LDR,
    OP_STR,
    OP_RTI,
    OP_NOT,
    OP_LDI,
    OP_STI,
    OP_JMP,
    OP_RES,
    OP_LEA,
    OP_TRAP,
    OP_RET = 0xC,
    OP_INVALID = 0xFFFF
} OpCode;

typedef struct
{
    uint8_t dr;
    uint8_t sr1;
    union
    {
        uint8_t sr2;
        int8_t imm5;
    };
    bool is_immediate;
} bin_op;

typedef struct
{
    uint8_t dr;
    int16_t pc_offset9;
} pc_offset9;

typedef struct
{
    uint8_t dr;
    uint8_t base_r;
    int16_t offset6;
} base_offset_instr;

typedef struct
{                           // bits 15:12
    uint16_t n : 1;         // bit 11
    uint16_t z : 1;         // bit 10
    uint16_t p : 1;         // bit 9
    int16_t pc_offset9 : 9; // bits 8:0 (signed)
} br;

typedef struct
{
    uint8_t sr;
    uint8_t base_r;
    uint16_t pc_offset11;
    bool is_pc_offset11;
} jsr;

typedef struct
{
    uint8_t base_r;
} jmp;

typedef struct
{
    uint8_t base_r;
} ret;

typedef struct
{
    uint8_t sr;
    uint8_t dr;
} not;

typedef struct
{
    uint8_t sr;
    uint16_t pc_offset9;
} store_instr;

typedef struct
{
    uint8_t sr;
    uint8_t base_r;
    uint16_t offset6;
} str;

typedef struct
{
    uint16_t trap_vec8;
} trap;

typedef struct
{
    OpCode op;
    union
    {
        bin_op add;
        bin_op and;
        pc_offset9 ld;
        pc_offset9 ldi;
        pc_offset9 lea;
        jsr jsr;
        br br;
        jmp jmp;
        base_offset_instr ldr;
        not not;
        store_instr st;
        store_instr sti;
        str str;
        trap trap;
        // ...other formats
    };
} Instruction;

void update_flags(VM *vm, uint16_t r)
{
    if (vm->reg[r] == 0)
    {
        vm->reg[R_COND] = FL_ZRO;
    }
    else if (vm->reg[r] >> 15)
    {
        vm->reg[R_COND] = FL_NEG;
    }
    else
    {
        vm->reg[R_COND] = FL_POS;
    }
}

Instruction decode(uint16_t cur_instr)
{
    // uint16_t cur_instr = vm->reg[R_PC];
    Instruction instr = {
        .op = (cur_instr >> 12) & 0xF};

    switch (instr.op)
    {
    case OP_BR:
        instr.br = (br){
            .n = (cur_instr >> 11) & 1,
            .z = (cur_instr >> 10) & 1,
            .p = (cur_instr >> 9) & 1,
            .pc_offset9 = sign_extend(cur_instr & 0x1FF, 9)};
        break;

    case OP_ADD:
    {
        uint8_t dr = (cur_instr >> 9) & 0x7;
        uint8_t sr1 = (cur_instr >> 6) & 0x7;
        uint8_t is_imm = get_bit_at_position(cur_instr, ADD_MODE_BIT);

        instr.add = (bin_op){
            .dr = dr,
            .sr1 = sr1,
            .is_immediate = is_imm,
        };
        if (is_imm)
        {
            instr.add.imm5 = sign_extend(cur_instr & 0x1F, 5);
        }
        else
        {
            instr.add.sr2 = cur_instr & 0x7;
        }
        break;
    }

    case OP_LD:
        instr.ld = (pc_offset9){
            .dr = (cur_instr >> 9) & 0x7,
            .pc_offset9 = sign_extend(cur_instr & 0x1FF, 9)};
        break;

    case OP_ST:
        instr.st = (store_instr){
            .sr = (cur_instr >> 9) & 0x7,
            .pc_offset9 = sign_extend(cur_instr & 0x1FF, 9)};
        break;

    case OP_JSR:
        instr.jsr = (jsr){
            .pc_offset11 = sign_extend(cur_instr & 0x7FF, 11),
            .is_pc_offset11 = get_bit_at_position(cur_instr, JSR_MODE_BIT),
            .base_r = (cur_instr >> 6) & 0x7};
        break;

    case OP_AND:
    {
        uint8_t dr = (cur_instr >> 9) & 0x7;
        uint8_t sr1 = (cur_instr >> 6) & 0x7;
        uint8_t is_imm = get_bit_at_position(cur_instr, ADD_MODE_BIT);

        instr.and = (bin_op){
            .dr = dr,
            .sr1 = sr1,
            .is_immediate = is_imm};
        if (is_imm)
        {
            instr.and.imm5 = sign_extend(cur_instr & 0x1F, 5);
        }
        else
        {
            instr.and.sr2 = cur_instr & 0x7;
        }
        break;
    }

    case OP_LDR:
        instr.ldr = (base_offset_instr){
            .dr = (cur_instr >> 9) & 0x7,
            .base_r = (cur_instr >> 6) & 0x7,
            .offset6 = sign_extend(cur_instr & 0x3F, 6)};
        break;

    case OP_STR:
        instr.str = (str){
            .sr = (cur_instr >> 9) & 0x7,
            .base_r = (cur_instr >> 6) & 0x7,
            .offset6 = sign_extend(cur_instr & 0x3F, 6)};
        break;

    case OP_RTI:
        // Typically reserved / unused; mark as invalid
        instr.op = OP_INVALID;
        break;

    case OP_NOT:
        instr.not = (not){
            .dr = (cur_instr >> 9) & 0x7,
            .sr = (cur_instr >> 6) & 0x7};
        break;

    case OP_LDI:
        instr.ldi = (pc_offset9){
            .dr = (cur_instr >> 9) & 0x7,
            .pc_offset9 = sign_extend(cur_instr & 0x1FF, 9)};
        break;

    case OP_STI:
        instr.st = (store_instr){
            .sr = (cur_instr >> 9) & 0x7,
            .pc_offset9 = sign_extend(cur_instr & 0x1FF, 9)};
        break;

    case OP_JMP:
        uint16_t baseR = (cur_instr >> 6) & 0x7;
        if (baseR == 0x7)
        {
            instr.jmp = (jmp){
                .base_r = 0x7};
        }
        else
        {
            instr.jmp = (jmp){
                .base_r = (cur_instr >> 6) & 0x7};
        }
        break;

    case OP_RES:
        // Reserved (unused); mark as invalid
        instr.op = OP_INVALID;
        break;

    case OP_LEA:
        instr.lea = (pc_offset9){
            .dr = (cur_instr >> 9) & 0x7,
            .pc_offset9 = sign_extend(cur_instr & 0x1FF, 9)};
        break;

    case OP_TRAP:
        instr.trap = (trap){
            .trap_vec8 = cur_instr & 0xFF};
        break;

        // case OP_RET:
        //     instr.ret = (ret){
        //         .base_r = 0x7};
        //     break;

    default:
        instr.op = OP_INVALID;
        break;
    }

    return instr;
}

int check_key()
{
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    struct timeval timeout = {0}; // No wait (poll)
    return select(1, &readfds, NULL, NULL, &timeout) > 0;
}

static inline void reg_write(VM *vm, uint16_t dr, uint16_t data) { vm->reg[dr] = data; }

static inline uint16_t reg_read(VM *vm, uint16_t sr) { return vm->reg[sr]; }

static inline void mem_write(VM *vm, uint16_t dr, uint16_t data) { vm->mem[dr] = data; }

static inline uint16_t mem_read(VM *vm, uint16_t address)
{
    if (address == KBDR)
    {
        vm->mem[KBSR] = 0;
    }

    return vm->mem[address];
}

void load_program(VM *vm, u_int16_t *program, size_t size)
{
    const uint16_t origin = 0x3000;

    for (size_t i = 0; i < size; ++i)
    {
        vm->mem[origin + i] = program[i];
    }

    vm->reg[R_PC] = origin;

    printf("PC = 0x%04X\n", vm->reg[R_PC]);
    for (int i = origin; i < origin + size; i++)
    {
        printf("mem[0x%04X] = 0x%04X\n", i, vm->mem[i]);
    }
}

int getch_async()
{
    struct termios oldt, newt;
    int ch;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if (ch != EOF)
        return ch;
    return 0;
}

void check_key_input(VM *vm)
{
    int c = getch_async();
    if (c > 0)
    {
        vm->mem[KBSR] = 0x8000;
        vm->mem[KBDR] = (uint16_t)c;
    }
    else
    {
        vm->mem[KBSR] = 0;
    }
}

void run(VM *vm)
{
    enum
    {
        PC_START = 0x3000
    };
    vm->reg[R_PC] = PC_START;
    vm->reg[R_COND] = FL_ZRO;

    int running = 1;

    while (running)
    {

        int ch = getch_async();

        if (vm->mem[KBSR] == 0)
        {
            if (ch > 0)
            {
                vm->mem[KBSR] = 0x8000;
                vm->mem[KBDR] = (uint16_t)ch;
            }
        }

        uint16_t cur_instr = mem_read(vm, vm->reg[R_PC]);
        Instruction instr = decode(cur_instr);
        vm->reg[R_PC]++;

        switch (instr.op)
        {
        case OP_ADD:
            if (instr.add.is_immediate)
                reg_write(vm, instr.add.dr, vm->reg[instr.add.sr1] + instr.add.imm5);
            else
                reg_write(vm, instr.add.dr, vm->reg[instr.add.sr1] + vm->reg[instr.add.sr2]);
            update_flags(vm, instr.add.dr);
            break;

        case OP_AND:
            if (instr.and.is_immediate)
                reg_write(vm, instr.and.dr, vm->reg[instr.and.sr1] & instr.and.imm5);
            else
                reg_write(vm, instr.and.dr, vm->reg[instr.and.sr1] & vm->reg[instr.and.sr2]);
            update_flags(vm, instr.and.dr);
            break;

        case OP_NOT:
            reg_write(vm, instr.not.dr, ~vm->reg[instr.not.sr]);
            update_flags(vm, instr.not.dr);
            break;

        case OP_BR:
            if ((instr.br.n && (vm->reg[R_COND] & FL_NEG)) ||
                (instr.br.z && (vm->reg[R_COND] & FL_ZRO)) ||
                (instr.br.p && (vm->reg[R_COND] & FL_POS)))
            {
                vm->reg[R_PC] += instr.br.pc_offset9;
            }
            break;

        case OP_JMP:
            vm->reg[R_PC] = vm->reg[instr.jmp.base_r];
            break;

        case OP_JSR:
            reg_write(vm, R_R7, vm->reg[R_PC]);
            if (instr.jsr.is_pc_offset11)
                vm->reg[R_PC] += instr.jsr.pc_offset11;
            else
                vm->reg[R_PC] = vm->reg[instr.jsr.base_r];
            break;

        case OP_LD:
            reg_write(vm, instr.ld.dr, mem_read(vm, vm->reg[R_PC] + instr.ld.pc_offset9));
            update_flags(vm, instr.ld.dr);
            break;

        case OP_LDI:
            printf("address: %i\n", vm->reg[R_PC] + instr.ldi.pc_offset9);
            uint16_t t = mem_read(vm, vm->reg[R_PC] + instr.ldi.pc_offset9);
            reg_write(vm, instr.ldi.dr, mem_read(vm, mem_read(vm, vm->reg[R_PC] + instr.ldi.pc_offset9)));
            update_flags(vm, instr.ldi.dr);
            break;

        case OP_LDR:
            reg_write(vm, instr.ldr.dr, mem_read(vm, vm->reg[instr.ldr.base_r] + instr.ldr.offset6));
            update_flags(vm, instr.ldr.dr);
            break;

        case OP_LEA:
            reg_write(vm, instr.lea.dr, vm->reg[R_PC] + instr.lea.pc_offset9);
            update_flags(vm, instr.lea.dr);
            break;

        case OP_ST:
            mem_write(vm, vm->reg[R_PC] + instr.st.pc_offset9, vm->reg[instr.st.sr]);
            update_flags(vm, instr.lea.dr);
            break;

        case OP_STI:
            mem_write(vm, mem_read(vm, vm->reg[R_PC] + instr.st.pc_offset9), vm->reg[instr.st.sr]);
            break;

        case OP_STR:
            mem_write(vm, vm->reg[instr.str.base_r] + instr.str.offset6, vm->reg[instr.str.sr]);
            break;

        case OP_TRAP:
        {
            reg_write(vm, R_R7, vm->reg[R_PC]);

            uint16_t trap_vector = instr.trap.trap_vec8;
            uint16_t trap_routine_address = mem_read(vm, trap_vector);

            vm->reg[R_PC] = trap_routine_address;

            if (trap_vector == 0x25)
            {
                running = 0;
            }
            break;
        }

        case OP_RES:
        case OP_RTI:
        default:
            // Invalid or OS-level instruction
            break;
        }
    }
}
