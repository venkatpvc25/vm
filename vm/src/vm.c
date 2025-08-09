
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

#define DSR_READY_MASK 0x7FFF

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
    else if (address == DSR)
    {
        return vm->mem[DSR];
    }

    return vm->mem[address];
}

void load_program(VM *vm, uint16_t *program, size_t size)
{
    size_t i = 0;

    while (i < size)
    {
        uint16_t origin = program[i++];
        printf("Loading block at origin 0x%04X\n", origin);

        while (i < size && (program[i] & 0xF000) != 0xF000)
        {
            vm->mem[origin++] = program[i++];
        }
    }

    vm->reg[R_PC] = 0x3000;
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

void trap_out(VM *vm)
{
    vm->mem[DSR] &= DSR_READY_MASK;
    uint16_t ch = vm->reg[0];
    putchar((char)ch);
    fflush(stdout);
    vm->mem[DSR] |= 0x8000;
    vm->mem[DSR] &= DSR_READY_MASK;
    vm->reg[R_PC] = vm->reg[R_R7];
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

        uint16_t pc = vm->reg[R_PC];

        int ch = getch_async();

        if (vm->mem[KBSR] == 0)
        {
            if (ch > 0)
            {
                vm->mem[KBSR] = 0x8000;
                vm->mem[KBDR] = (uint16_t)ch;
            }
        }

        uint16_t cur_instr = mem_read(vm, pc);
        Instruction instr = decode(cur_instr);
        vm->reg[R_PC]++;

        if (pc == 0x0106)
        {
            trap_out(vm);
            continue;
        }

        switch (instr.op)
        {
        case OP_ADD:
        {
            uint16_t left = vm->reg[instr.add.sr1];
            uint16_t result;

            if (instr.add.is_immediate)
            {
                printf("ADD (immediate): R%d = R%d (%d) + %d\n", instr.add.dr, instr.add.sr1, left, instr.add.imm5);
                result = left + instr.add.imm5;
            }
            else
            {
                uint16_t right = vm->reg[instr.add.sr2];
                printf("ADD (register): R%d = R%d (%d) + R%d (%d)\n", instr.add.dr, instr.add.sr1, left, instr.add.sr2, right);
                result = left + right;
            }

            reg_write(vm, instr.add.dr, result);
            update_flags(vm, instr.add.dr);
            break;
        }

        case OP_AND:
        {
            uint16_t left = vm->reg[instr.and.sr1];
            uint16_t result;

            if (instr.and.is_immediate)
            {
                printf("AND (immediate): R%d = R%d (%d) & %d\n", instr.and.dr, instr.and.sr1, left, instr.and.imm5);
                result = left & instr.and.imm5;
            }
            else
            {
                uint16_t right = vm->reg[instr.and.sr2];
                printf("AND (register): R%d = R%d (%d) & R%d (%d)\n", instr.and.dr, instr.and.sr1, left, instr.and.sr2, right);
                result = left & right;
            }

            reg_write(vm, instr.and.dr, result);
            update_flags(vm, instr.and.dr);
            break;
        }

        case OP_NOT:
        {
            uint16_t value = vm->reg[instr.not.sr];
            uint16_t result = ~value;
            printf("NOT: R%d = ~R%d (%d) = %d\n", instr.not.dr, instr.not.sr, value, result);

            reg_write(vm, instr.not.dr, result);
            update_flags(vm, instr.not.dr);
            break;
        }

        case OP_BR:
        {
            uint16_t cond_flags = vm->reg[R_COND];
            int16_t offset = instr.br.pc_offset9;

            bool should_branch =
                (instr.br.n && (cond_flags & FL_NEG)) ||
                (instr.br.z && (cond_flags & FL_ZRO)) ||
                (instr.br.p && (cond_flags & FL_POS));

            printf("BR: cond_flags=0x%X, offset=%d, should_branch=%s\n", cond_flags, offset, should_branch ? "true" : "false");

            if (should_branch)
            {
                vm->reg[R_PC] += offset;
                printf("BR taken: new PC=0x%04X\n", vm->reg[R_PC]);
            }
            break;
        }

        case OP_JMP:
        {
            uint16_t base_address = vm->reg[instr.jmp.base_r];
            printf("JMP: PC <- R%d (0x%04X)\n", instr.jmp.base_r, base_address);
            vm->reg[R_PC] = base_address;
            break;
        }

        case OP_JSR:
        {
            uint16_t return_address = vm->reg[R_PC];
            reg_write(vm, R_R7, return_address);

            if (instr.jsr.is_pc_offset11)
            {
                int16_t offset = instr.jsr.pc_offset11;
                vm->reg[R_PC] += offset;
                printf("JSR (PC offset): PC <- PC + %d = 0x%04X\n", offset, vm->reg[R_PC]);
            }
            else
            {
                uint16_t base_address = vm->reg[instr.jsr.base_r];
                vm->reg[R_PC] = base_address;
                printf("JSR (register): PC <- R%d (0x%04X)\n", instr.jsr.base_r, base_address);
            }
            break;
        }

        case OP_LD:
        {
            uint16_t addr = vm->reg[R_PC] + instr.ld.pc_offset9;
            uint16_t value = mem_read(vm, addr);
            printf("LD: Load from 0x%04X value 0x%04X into R%d\n", addr, value, instr.ld.dr);

            reg_write(vm, instr.ld.dr, value);
            update_flags(vm, instr.ld.dr);
            break;
        }

        case OP_LDI:
        {
            uint16_t addr1 = vm->reg[R_PC] + instr.ldi.pc_offset9;
            uint16_t addr2 = mem_read(vm, addr1);
            uint16_t value = mem_read(vm, addr2);

            printf("LDI: addr1=0x%04X, addr2=0x%04X, value=0x%04X into R%d\n", addr1, addr2, value, instr.ldi.dr);

            reg_write(vm, instr.ldi.dr, value);
            update_flags(vm, instr.ldi.dr);
            break;
        }

        case OP_LDR:
        {
            uint16_t base = vm->reg[instr.ldr.base_r];
            int16_t offset = instr.ldr.offset6;
            uint16_t addr = base + offset;
            uint16_t value = mem_read(vm, addr);

            printf("LDR: Load from 0x%04X value 0x%04X into R%d\n", addr, value, instr.ldr.dr);

            reg_write(vm, instr.ldr.dr, value);
            update_flags(vm, instr.ldr.dr);
            break;
        }

        case OP_LEA:
        {
            uint16_t addr = vm->reg[R_PC] + instr.lea.pc_offset9;
            printf("LEA: Load address 0x%04X into R%d\n", addr, instr.lea.dr);

            reg_write(vm, instr.lea.dr, addr);
            update_flags(vm, instr.lea.dr);
            break;
        }

        case OP_ST:
        {
            uint16_t addr = vm->reg[R_PC] + instr.st.pc_offset9;
            uint16_t value = vm->reg[instr.st.sr];
            printf("ST: Store R%d (0x%04X) into memory address 0x%04X\n", instr.st.sr, value, addr);

            mem_write(vm, addr, value);
            break;
        }

        case OP_STI:
        {
            uint16_t pc = vm->reg[R_PC];
            uint16_t addr1 = pc + instr.st.pc_offset9;
            uint16_t addr2 = mem_read(vm, addr1);
            uint16_t value = vm->reg[instr.st.sr];

            printf("STI: pc=0x%04X, addr1=0x%04X (indirect), addr2=0x%04X, value=0x%04X (R%d)\n",
                   pc, addr1, addr2, value, instr.st.sr);

            mem_write(vm, addr2, value);
            break;
        }

        case OP_STR:
        {
            uint16_t base = vm->reg[instr.str.base_r];
            int16_t offset = instr.str.offset6;
            uint16_t addr = base + offset;
            uint16_t value = vm->reg[instr.str.sr];

            printf("STR: Store R%d (0x%04X) into memory address 0x%04X (base R%d + offset %d)\n",
                   instr.str.sr, value, addr, instr.str.base_r, offset);

            mem_write(vm, addr, value);
            break;
        }

        case OP_TRAP:
        {
            uint16_t return_address = vm->reg[R_PC];
            reg_write(vm, R_R7, return_address);

            uint16_t trap_vector = instr.trap.trap_vec8;
            uint16_t trap_routine_address = mem_read(vm, trap_vector);

            printf("TRAP: trap_vector=0x%02X, handler=0x%04X\n", trap_vector, trap_routine_address);

            vm->reg[R_PC] = trap_routine_address;

            if (trap_vector == 0x25)
            {
                printf("TRAP HALT called, stopping execution\n");
                running = 0;
            }
            break;
        }

        case OP_RES:
        case OP_RTI:
        default:
            // Invalid or OS-level instruction, do nothing
            printf("Unknown or reserved opcode: 0x%X\n", instr.op);
            break;
        }
    }
}