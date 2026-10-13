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

int get_register(regs_struct *regs, REGISTER reg, UWORD **reg_output) {
    switch (reg) {
    case r15:
        *reg_output = &regs->r15;
        break;

    case r14:
        *reg_output = &regs->r14;
        break;

    case r13:
        *reg_output = &regs->r13;
        break;

    case r12:
        *reg_output = &regs->r12;
        break;

    case rbp:
        *reg_output = &regs->rbp;
        break;

    case rbx:
        *reg_output = &regs->rbx;
        break;

    case r11:
        *reg_output = &regs->r11;
        break;

    case r10:
        *reg_output = &regs->r10;
        break;

    case r9:
        *reg_output = &regs->r9;
        break;

    case r8:
        *reg_output = &regs->r8;
        break;

    case rax:
        *reg_output = &regs->rax;
        break;

    case rcx:
        *reg_output = &regs->rcx;
        break;

    case rdx:
        *reg_output = &regs->rdx;
        break;

    case rsi:
        *reg_output = &regs->rsi;
        break;

    case rdi:
        *reg_output = &regs->rdi;
        break;

    case orig_rax:
        *reg_output = &regs->orig_rax;
        break;

    case rip:
        *reg_output = &regs->rip;
        break;

    case cs:
        *reg_output = &regs->cs;
        break;

    case eflags:
        *reg_output = &regs->eflags;
        break;

    case rsp:
        *reg_output = &regs->rsp;
        break;

    case ss:
        *reg_output = &regs->ss;
        break;

    case fs_base:
        *reg_output = &regs->fs_base;
        break;

    case gs_base:
        *reg_output = &regs->gs_base;
        break;

    case ds:
        *reg_output = &regs->ds;
        break;

    case es:
        *reg_output = &regs->es;
        break;

    case fs:
        *reg_output = &regs->fs;
        break;

    case gs:
        *reg_output = &regs->gs;
        break;

    default:
        return 1;
    }

    return 0;
}

void print_registers(regs_struct *regs) {
    int res;
    for (REGISTER reg = 0; reg < NUM_REGISTERS; reg++) {
        UWORD *p_reg;
        get_register(regs, reg, &p_reg);
        fprintf(stdout, "%-12s:%llx\n", registerAbbr[reg], *p_reg);
    }
} // cmdline