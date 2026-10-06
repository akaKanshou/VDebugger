#include "debugger.h"
#include "cmdline.h"

#include <linux/limits.h>
#include <sys/errno.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>

// TODO: Move all printing to seperate library
// TODO: Error handling ptrace and waitpid calls.
// TODO: Change all variables to snake_case
// TODO: ESRCH on successful termination - fix it

int new_debugger(int c_pid, char *file, Debugger **__dbg) {
    Debugger *dbg = (Debugger *)malloc(sizeof(Debugger));
    dbg->c_pid = c_pid;
    dbg->breakpoints = hashmap_new(sizeof(Breakpoint), 0, 0, 0, breakpoint_hash,
                                   breakpoint_cmp, NULL, NULL);
    int res = get_load_address(c_pid, &dbg->load_address);
    if (res) return res;
    res = init_dwarf(file, &dbg->dwarf_dbg);
    if (res) return res;
    *__dbg = dbg;
    return 0;
}

void free_debugger(Debugger *dbg) {
    dwarf_finish(dbg->dwarf_dbg);
    free(dbg);
}

int get_load_address(int c_pid, WORD *load_addr) {
    char sysCall[64], memOffsetHex[64];
    sprintf(sysCall, "cat /proc/%i/maps | head -c 16", c_pid);
    FILE *fpipe = popen(sysCall, "r");
    if (!fpipe) return 1;
    if (fgets(memOffsetHex, 64, fpipe) == NULL) return 1;
    *load_addr = strtoll(memOffsetHex, NULL, 16);
    return 0;
}

int wait_for_signal(Debugger *dbg, int *status, int options) {
    errno = 0;
    if (waitpid(dbg->c_pid, status, options) == -1) return 3;

    // TODO: move to seperate function
    siginfo_t info;
    int res = 0;
    res = get_siginfo(dbg, &info);
    if (res) {
        return res;
    }
    switch (info.si_signo) {
    case SIGTRAP:
        return handle_sigtrap(dbg, &info);
        break;
    default:
        fprintf(stdout, "\nUnhandled signal: %x\n", info.si_code);
        break;
    }

    return 0;
}

int handle_sigtrap(Debugger *dbg, siginfo_t *info) {
    int res = 0;
    switch (info->si_code) {
    case SI_KERNEL:
    case TRAP_BRKPT:
        regs_struct regs;
        res = get_regs_struct(dbg, &regs);
        if (res) return res;

        UWORD *pc = get_register(&regs, rip); // TODO: change io
        *pc = *pc - 1;

        res = set_regs_struct(dbg, &regs);
        if (res) return res;

        break;
    case TRAP_TRACE:
    }
    return 0;
}

int get_siginfo(Debugger *dbg, siginfo_t *info) {
    if (ptrace(PTRACE_GETSIGINFO, dbg->c_pid, NULL, info) == -1) {
        return 2;
    }
    return 0;
}

int run_debugger(Debugger *dbg) {
    if (dbg == NULL) {
        return 1;
    }

    int status, options = 0, res;
    res = wait_for_signal(dbg, &status, options);
    if (res == 3) {
        fprintf(stderr, "Error on wait_for_signal: %s\n",
                get_waitpid_err(errno)); // cmdline
        return res;
    } else if (res == 2) {
        fprintf(stderr, "Error on ptrace: %s\n",
                get_ptrace_err(errno)); // cmdline
        return res;
    } else if (res) {
        return res;
    }

    COMMAND cmnd;
    Buffer *buffer = new_buffer(CMD_MAX_SIZE), *line = new_buffer(0);
    while (cmnd != EXIT && poll_input(line)) {
        while (res = parse_input(buffer, line)) {
            if (res < 0) {
                cmnd = INVALID_CMD;
                reset_seek(buffer);
                continue;
            }

            cmnd = match_command(next_token(buffer));
            if (cmnd == EXIT) {
                break;
            }

            int debugee_terminated = 0,
                res = handle_command(dbg, cmnd, buffer, &debugee_terminated);
            if (!res) {
                fprintf(stdout, "\nOK\n"); // cmdline
            } else {
                fprintf(stdout, "\n!x!\n"); // cmdline
                return 0;
            }

            if (debugee_terminated) {
                fprintf(stdout, "Debugee terminated\n"); // cndline
            }
            reset_seek(buffer);
        }

        linenoiseFree(line->data);
        reset_seek(line);
    }

    free(buffer->data);
    free(buffer);
    free(line);
    return 0;
}

int debug_continue(Debugger *dbg, int *debugee_terminated) {
    int res = 0;
    if (true) /*TODO: if exists breakpoint*/ {
        res = step_over_breakpoint(dbg);
        if (res) {
            return res;
        }
    }

    if (ptrace(PTRACE_CONT, dbg->c_pid, NULL, NULL) == -1) {
        return 2;
    }

    // TODO: waiting should be seperate function
    int status, options = 0;
    res = wait_for_signal(dbg, &status, options);
    if (res == 3) {
        fprintf(stderr, "Error on wait_for_signal: %s\n",
                get_waitpid_err(errno)); // cmdline
        return res;
    } else if (res == 2) {
        fprintf(stderr, "Error on ptrace: %s\n",
                get_ptrace_err(errno)); // cmdline
        return res;
    } else if (res) {
        return res;
    }

    if (WIFEXITED(status) || WIFSIGNALED(status)) {
        *debugee_terminated = 1;
    }

    return 0;
}

int handle_command(Debugger *dbg, COMMAND cmnd, Buffer *buffer,
                   int *debugee_terminated) {
    int res = 0;
    switch (cmnd) {
    case CONTINUE:
        res = debug_continue(dbg, debugee_terminated);
        break;
    case BREAKPOINT:
        res = handle_breakpoint(dbg, buffer);
        break;
    case REG:
        res = handle_register(dbg, buffer);
        break;
    case STEP:
        res = handle_step(dbg, buffer);
        break;
    case WHERE:
        res = handle_where(dbg, buffer);
        break;
    case FINISH:
        res = step_out(dbg);
        break;
    default:
        fprintf(stdout, "Unknown command"); // cmdline
        return 1;
    }

    return res;
}

// =======================================
// BREAKPOINT
// =======================================

// TODO: application functions need serious refactoring and break down
int handle_breakpoint(Debugger *dbg, Buffer *buffer) {
    BREAKPOINT_OPTIONS action = match_breakpoint_option(next_token(buffer)),
                       mode = match_breakpoint_option(next_token(buffer));
    if (action == INVALID_BREAKPOINT_OPT || mode == INVALID_BREAKPOINT_OPT)
        return 1;

    char *argToken = next_token(buffer), *argEnd;
    if (!argToken) return 1;

    WORD arg;
    switch (mode) {
    case BREAKPOINT_ARG_MEMADDR:
        arg = strtoll(argToken, &argEnd, 16);
        if (*argEnd) return 1;
        break;
    case BREAKPOINT_ARG_LINENUM:
        char *file_name = argToken;
        argToken = next_token(buffer);
        arg = strtoll(argToken, &argEnd, 10);
        if (*argEnd) return 1;
        Dwarf_Addr addr;
        Dwarf_Error error;
        int res = get_addr_from_source_line(file_name, arg, dbg->dwarf_dbg,
                                            &addr, &error);
        if (res == DW_DLV_ERROR) {
            fprintf(stdout, "error:%s\n", dwarf_errmsg(error));
            return 1;
        } else if (res != DW_DLV_OK) {
            fprintf(stdout, "noentry\n");
            return 1;
        }

        arg = addr;
        break;
    default:
        return 1;
    }
    arg += dbg->load_address;

    int res;
    Breakpoint *breakpoint;
    res = get_breakpoint_at_addr(dbg, arg, &breakpoint);
    if (res) {
        return res;
    }

    switch (action) {
    case ENABLE_BREAKPOINT:
        res = enable_breakpoint(dbg, breakpoint);
        break;
    case DISABLE_BREAKPOINT:
        res = disable_breakpoint(dbg, breakpoint);
        break;
    }

    free(breakpoint);
    return res;
}

Breakpoint *make_breakpoint(WORD memAddr) {
    Breakpoint *breakpoint = (Breakpoint *)malloc(sizeof(Breakpoint));
    if (!breakpoint) return NULL;

    breakpoint->mem_addr = memAddr;
    breakpoint->saved_data = 0;
    breakpoint->enabled = false;
    return breakpoint;
}

int copy_breakpoint(Breakpoint *dest, const Breakpoint *src) {
    if (src == NULL) return 1;
    dest->enabled = src->enabled;
    dest->mem_addr = src->mem_addr;
    dest->saved_data = src->saved_data;
    dest->setKey = dest->setKey;
}

// TODO: free allocated breakpoints at every point
int get_breakpoint_at_addr(Debugger *dbg, WORD memAddr,
                           Breakpoint **breakpoint) {
    *breakpoint = make_breakpoint(memAddr);
    if (!breakpoint) return 1;
    const Breakpoint *bp_in_map = hashmap_get(dbg->breakpoints, *breakpoint);
    if (copy_breakpoint(*breakpoint, bp_in_map) > 0) {
        free(*breakpoint);
        *breakpoint = NULL;
    }
    return 0;
}

int enable_breakpoint(Debugger *dbg, Breakpoint *breakpoint) {
    errno = 0;
    WORD data = ptrace(PTRACE_PEEKDATA, dbg->c_pid, breakpoint->mem_addr, NULL);
    if (data == -1 && errno) {
        return 2;
    }

    breakpoint->saved_data = data;

    data = (data & ~0xff) | 0xcc;

    if (ptrace(PTRACE_POKEDATA, dbg->c_pid, breakpoint->mem_addr, data, NULL) ==
        -1) {
        return 2;
    }

    breakpoint->enabled = true;
    return 0;
}

int disable_breakpoint(Debugger *dbg, Breakpoint *breakpoint) {
    if (ptrace(PTRACE_POKEDATA, dbg->c_pid, breakpoint->mem_addr,
               breakpoint->saved_data, NULL) == -1) {
        return 2;
    }

    breakpoint->enabled = false;
    return 0;
}

int breakpoint_cmp(const void *a, const void *b, void *udata) {
    const Breakpoint *memA = a;
    const Breakpoint *memB = b;
    return memA->mem_addr != memB->mem_addr;
}

uint64_t breakpoint_hash(const void *item, uint64_t seed0, uint64_t seed1) {
    const Breakpoint *memA = item;
    return hashmap_murmur(&memA->mem_addr, sizeof(memA->mem_addr), seed0,
                          seed1);
}

// =======================================

// =======================================
// REGISTERS
// =======================================

// TODO: Refactor this function. Does too much.
int handle_register(Debugger *dbg, Buffer *buffer) {
    REGISTER_OPTIONS action = match_register_option(next_token(buffer)),
                     mode = match_register_option(next_token(buffer));

    if (action == INVALID_REGISTER_OPT || mode == INVALID_REGISTER_OPT) {
        return 1;
    }

    char *arg = next_token(buffer);
    int res;

    regs_struct regs;
    REGISTER reg;
    if (res = get_regs_struct(dbg, &regs)) {
        return res;
    }

    switch (action) {
    case READ_REGISTER:
        if (mode == REGISTER_ARG_ALL) {
            if (arg) {
                fprintf(stdout, "invalid arg %s\n", arg);
                return 1;
            }

            print_registers(&regs);
            return 0;
        }

        if (!arg) return 1;

        if (mode == REGISTER_ARG_ABBR) {
            reg = abbr_to_reg(arg);
        } else if (mode == REGISTER_ARG_DWARF) {
            int dwarfn = strtol(arg, &arg, 16);
            if (arg + 1 < buffer->data + buffer->rseek) {
                return 1;
            }
            reg = DW_to_reg(dwarfn);
        }
        if (reg == NO_SUCH_REGISTER) return 1;

        fprintf(stdout, "value :%llx\n", *get_register(&regs, reg)); // cmdline

        return 0;
    case WRITE_REGISTER:
        if (!arg) return 1;

        if (mode == REGISTER_ARG_ABBR) {
            reg = abbr_to_reg(arg);
        } else if (mode == REGISTER_ARG_DWARF) {
            int dwarfn = strtol(arg, &arg, 10);
            if (arg + 1 < buffer->data + buffer->rseek) {
                return 1;
            }
            reg = DW_to_reg(dwarfn);
        }
        if (reg == NO_SUCH_REGISTER) return 1;

        arg = next_token(buffer);
        if (!arg) return 1;
        WORD value = strtoll(arg, &arg, 10);
        if (arg + 1 < buffer->data + buffer->rseek) {
            return 1;
        }

        set_reg_value(dbg, reg, value);

        return 0;
    }

    return 1;
}

int get_regs_struct(Debugger *dbg, regs_struct *regs) {
    if (ptrace(PTRACE_GETREGS, dbg->c_pid, NULL, regs) == -1) {
        return 2;
    }
    return 0;
}

int set_regs_struct(Debugger *dbg, regs_struct *regs) {
    if (ptrace(PTRACE_SETREGS, dbg->c_pid, NULL, regs) == -1) {
        return 2;
    }
    return 0;
}

int get_reg_value(Debugger *dbg, REGISTER reg, WORD *value) {
    int res;

    regs_struct regs;
    if ((res = get_regs_struct(dbg, &regs)) > 0) return res;

    UWORD *regl = get_register(&regs, reg);
    if (!regl) return NO_SUCH_REGISTER;

    *value = *regl;
    return 0;
}

int set_reg_value(Debugger *dbg, REGISTER reg, WORD value) {
    int res;

    regs_struct regs;
    if (res = get_regs_struct(dbg, &regs)) return res;

    UWORD *regl = get_register(&regs, reg);
    if (!regl) return NO_SUCH_REGISTER;

    *regl = value;
    if (res = set_regs_struct(dbg, &regs)) return res;

    return 0;
}

// =======================================

// =======================================
// STEP
// =======================================

int single_step(Debugger *dbg) {
    if (ptrace(PTRACE_SINGLESTEP, dbg->c_pid, NULL, NULL) == -1) {
        fprintf(stderr, "Error Single-Stepping: %s\n", get_ptrace_err(errno));
        return 2;
    }
    return 0;
}

int step_over_breakpoint(Debugger *dbg) {
    int res;
    regs_struct regs;
    get_regs_struct(dbg, &regs);

    UWORD *program_counter = get_register(&regs, rip);

    Breakpoint *breakpoint;
    res = get_breakpoint_at_addr(dbg, *program_counter, &breakpoint);
    if (res) {
        return res;
    }
    if (!breakpoint || !breakpoint->enabled) {
        // No breakpoint to step over, or is not enabled.
        return 0;
    }

    res = disable_breakpoint(dbg, breakpoint); // Disable
    if (res) return res;

    res = single_step(dbg); // Single step
    if (res) return res;

    int status, options = 0;
    res = wait_for_signal(dbg, &status, options);
    if (res == 3) {
        fprintf(stderr, "Error on wait_for_signal: %s\n",
                get_waitpid_err(errno)); // cmdline
        return res;
    } else if (res == 2) {
        fprintf(stderr, "Error on ptrace: %s\n",
                get_ptrace_err(errno)); // cmdline
        return res;
    } else if (res) {
        return res;
    }

    res = enable_breakpoint(dbg, breakpoint); // Enable breakpoint
    if (res) return res;

    return 0;
}

int handle_step(Debugger *dbg, Buffer *buffer) {
    char *arg = next_token(buffer);
    if (!arg) return 1;

    WORD times = strtoll(arg, &arg, 10);
    if (arg + 1 < buffer->data + buffer->rseek) return 1;

    int res = 0;
    while (times--) {
        UWORD pc;
        get_reg_value(dbg, rip, &pc);
        Breakpoint *breakpoint;
        res = get_breakpoint_at_addr(dbg, pc, &breakpoint);
        if (res) {
            return res;
        }
        if (breakpoint && breakpoint->enabled) {
            if (res = step_over_breakpoint(dbg)) {
                break;
            }
        } else {
            if (res = single_step(dbg)) {
                break;
            }

            int status, option = 0;
            res = wait_for_signal(dbg, &status, option);
            if (res == 3) {
                fprintf(stderr, "Error on wait_for_signal: %s\n",
                        get_waitpid_err(errno)); // cmdline
                return res;
            } else if (res == 2) {
                fprintf(stderr, "Error on ptrace: %s\n",
                        get_ptrace_err(errno)); // cmdline
                return res;
            } else if (res) {
                return res;
            }
        }
    }
    if (res) return res;
    return 0;
}

// =======================================

// =======================================
// NEXT
// =======================================

// =======================================

// =======================================
// FINISH
// =======================================

int step_out(Debugger *dbg) {
    int res;
    UWORD frame_pointer;
    res = get_reg_value(dbg, rbp, &frame_pointer);
    if (res) return res;
    errno = 0;
    UWORD return_addr =
        ptrace(PTRACE_PEEKDATA, dbg->c_pid, frame_pointer + 8, NULL);
    if (return_addr == -1 && errno) {
        return 2;
    }

    Breakpoint *breakpoint;
    res = get_breakpoint_at_addr(dbg, return_addr, &breakpoint);
    if (res) {
        return res;
    }

    int temp = !breakpoint || !breakpoint->enabled;
    if (!breakpoint) {
        breakpoint = make_breakpoint(return_addr);
    }

    enable_breakpoint(dbg, breakpoint);

    // TODO: debugee_terminated atm, fix
    int debugee_terminated = 0;
    res = debug_continue(dbg, &debugee_terminated);
    if (res) {
        return res;
    }

    if (temp) {
        res = disable_breakpoint(dbg, breakpoint);
        if (res) return res;
    }

    return 0;
}

// =======================================

// =======================================
// Where
// =======================================

int handle_where(Debugger *dbg, Buffer *buffer) {
    char *arg = next_token(buffer);
    if (!arg) return 1;

    WHERE_OPTIONS mode = match_where_option(arg);
    int res = 0;

    WORD pc;
    res = get_reg_value(dbg, rip, &pc);
    if (res) return res;

    Dwarf_Error error;

    switch (mode) {
    case WHERE_ARG_ADDR:
        // cmdline
        fprintf(stdout, "Program Counter: %llx Offset: %llx\n", pc,
                pc - dbg->load_address);

        return 0;
    case WHERE_ARG_FUNC:

        Dwarf_Die sub_prog_die = 0;
        res = get_sub_prog_die_from_addr(pc - dbg->load_address, dbg->dwarf_dbg,
                                         &sub_prog_die, &error);

        if (res == DW_DLV_ERROR) {
            fprintf(stderr, "error: %s\n", dwarf_errmsg(error));
            dwarf_dealloc_error(dbg->dwarf_dbg, error);
            return 1;
        }

        char *func_name;
        res = get_sub_program_name(dbg->dwarf_dbg, sub_prog_die, &func_name,
                                   &error);

        dwarf_dealloc_die(sub_prog_die);
        if (res == DW_DLV_ERROR) {
            fprintf(stderr, "error: %s\n", dwarf_errmsg(error));
            dwarf_dealloc_error(dbg->dwarf_dbg, error);
            return 1;
        } else if (res == DW_DLV_NO_ENTRY) {
            fprintf(stdout, "Unknown name :(\n");
            return 0;
        }

        fprintf(stdout, "Current function: %s\n", func_name);
        return 0;
    case WHERE_ARG_LINE:

        Dwarf_Unsigned line_no;
        res = get_line_no_from_addr(pc - dbg->load_address, dbg->dwarf_dbg,
                                    &line_no, &error);
        if (res == DW_DLV_ERROR) {
            fprintf(stdout, "error: %s\n", dwarf_errmsg(error));
            return 1;
        } else if (res == DW_DLV_NO_ENTRY) {
            return 1;
        }

        fprintf(stdout, "Current line: %lli\n", line_no);
        return 0;
    default:
        return 1;
    }
}

// =======================================

// =======================================
// Errors
// =======================================

const char *get_waitpid_err(int err) {
    switch (err) {
    case EAGAIN:
        return "EAGAIN";
    case ECHILD:
        return "ECHILD";
    case EINVAL:
        return "EINVAL";
    case EINTR:
        return "EINTR";
    case ESRCH:
        return "ESRCH";
    default:
        return "Unknown err";
    }
}

const char *get_ptrace_err(int err) {
    switch (err) {
    case EBUSY:
        return "EBUSY";
    case EFAULT:
        return "EFAULT";
    case EINVAL:
        return "EINVAL";
    case EIO:
        return "EIO";
    case EPERM:
        return "EPERM";
    case ESRCH:
        return "ESRCH";
    default:
        return "Unknown err";
    }
}

// =======================================