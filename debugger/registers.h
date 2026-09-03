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

// loadDWARFMappings initializes the register info.
void loadDWARFMappings();

// regToDWARFN gets the DWARF register number of the given register.
int regToDWARFN(REGISTER reg);

// regToAbbr copies the abbreviated name of the given register to buf.
int regToAbbr(REGISTER reg, char *buf);

// abbrToReg gets the register number of the given register abbreviation.
REGISTER abbrToReg(const char *regAbbr);

// DWARFNToReg gets the register number of the given DWARF register number.
REGISTER DWARFNToReg(int DWARFN);

// getRegister returns the location of the register field in the given
// user_regs_struct struct.
// Returns NULL if reg value is invalid.
UWORD *getRegister(regs_struct *regs, REGISTER reg);

// printRegisters prints the given regs_struct.
void printRegisters(regs_struct *regs);

#endif