#ifndef REGISTERS_H
#define REGISTERS_H

#define NO_SUCH_REGISTER -2

#include <stdint.h>
#include <sys/user.h>

typedef struct user_regs_struct regs_struct;

typedef unsigned long long int UWORD;
typedef int64_t WORD;

typedef enum REGISTER {
    r15,
    r14,
    r13,
    r12,
    rbp,
    rbx,
    r11,
    r10,
    r9,
    r8,
    rax,
    rcx,
    rdx,
    rsi,
    rdi,
    orig_rax,
    rip,
    cs,
    eflags,
    rsp,
    ss,
    fs_base,
    gs_base,
    ds,
    es,
    fs,
    gs,
    NUM_REGISTERS,
} REGISTER;

// load_DW_mappings initializes the register info.
void load_DW_mappings();

// reg_to_DW gets the DWARF register number of the given register.
int reg_to_DW(REGISTER reg);

// reg_to_abbr copies the abbreviated name of the given register to buf.
int reg_to_abbr(REGISTER reg, char *buf);

// abbr_to_reg gets the register number of the given register abbreviation.
REGISTER abbr_to_reg(const char *regAbbr);

// DW_to_reg gets the register number of the given DWARF register number.
REGISTER DW_to_reg(int DWARFN);

// get_register returns the location of the register field in the given
// user_regs_struct struct.
// Returns NULL if reg value is invalid.
UWORD *get_register(regs_struct *regs, REGISTER reg);

// print_registers prints the given regs_struct.
void print_registers(regs_struct *regs);

#endif