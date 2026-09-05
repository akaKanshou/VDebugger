#include "registers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// DWARFMappings contains the mapping of the registers with their DWARF register
// number.
static int DWARFMappings[NUM_REGISTERS];

//  registerAbbr contains the mapping of the registers with their abbreviations.
static char registerAbbr[NUM_REGISTERS][12];

void load_DW_mappings() {
    char *dat = "r15,15"
                "r14,14"
                "r13,13"
                "r12,12"
                "rbp,6"
                "rbx,3"
                "r11,11"
                "r10,10"
                "r9,9"
                "r8,8"
                "rax,0"
                "rcx,2"
                "rdx,1"
                "rsi,4"
                "rdi,5"
                "orig_rax,-1"
                "rip,-1"
                "cs,51"
                "eflags,49"
                "rsp,7"
                "ss,52"
                "fs_base,58"
                "gs_base,59"
                "ds,53"
                "es,50"
                "fs,54"
                "gs,55";

    int regNum = 0, datSeek = 0, bufferSeek = 0;
    char buffer[16];
    while (dat[datSeek]) {
        while (dat[datSeek] != ',') {
            buffer[bufferSeek++] = dat[datSeek++];
        }
        buffer[bufferSeek] = '\0';
        datSeek++;

        strcpy(registerAbbr[regNum], buffer);
        DWARFMappings[regNum] = strtol(dat + datSeek, &dat, 10);

        regNum++;
        datSeek = bufferSeek = 0;
    }
}

int reg_to_DW(REGISTER reg) {
    if (reg < 0 || reg >= NUM_REGISTERS) {
        return NO_SUCH_REGISTER;
    }
    return DWARFMappings[reg];
}

int reg_to_abbr(REGISTER reg, char *buf) {
    if (reg < 0 || reg >= NUM_REGISTERS) {
        return NO_SUCH_REGISTER;
    }
    strcpy(buf, registerAbbr[reg]);
    return 0;
}

REGISTER abbr_to_reg(const char *abbr) {
    for (REGISTER i = 0; i < NUM_REGISTERS; i++) {
        if (!strcmp(abbr, registerAbbr[i])) return i;
    }
    return NO_SUCH_REGISTER;
}

REGISTER DW_to_reg(int DWARFN) {
    for (REGISTER i = 0; i < NUM_REGISTERS; i++) {
        if (DWARFMappings[i] == DWARFN) return i;
    }
    return NO_SUCH_REGISTER;
}

UWORD *get_register(regs_struct *regs, REGISTER reg) {
    switch (reg) {
    case r15:
        return &regs->r15;
    case r14:
        return &regs->r14;
    case r13:
        return &regs->r13;
    case r12:
        return &regs->r12;
    case rbp:
        return &regs->rbp;
    case rbx:
        return &regs->rbx;
    case r11:
        return &regs->r11;
    case r10:
        return &regs->r10;
    case r9:
        return &regs->r9;
    case r8:
        return &regs->r8;
    case rax:
        return &regs->rax;
    case rcx:
        return &regs->rcx;
    case rdx:
        return &regs->rdx;
    case rsi:
        return &regs->rsi;
    case rdi:
        return &regs->rdi;
    case orig_rax:
        return &regs->orig_rax;
    case rip:
        return &regs->rip;
    case cs:
        return &regs->cs;
    case eflags:
        return &regs->eflags;
    case rsp:
        return &regs->rsp;
    case ss:
        return &regs->ss;
    case fs_base:
        return &regs->fs_base;
    case gs_base:
        return &regs->gs_base;
    case ds:
        return &regs->ds;
    case es:
        return &regs->es;
    case fs:
        return &regs->fs;
    case gs:
        return &regs->gs;
    default:
        return NULL;
    }
}

void print_registers(regs_struct *regs) {
    for (REGISTER reg = 0; reg < NUM_REGISTERS; reg++) {
        fprintf(stdout, "%-12s:%llx\n", registerAbbr[reg],
                *get_register(regs, reg));
    }
} // cmdline