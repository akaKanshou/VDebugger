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
// TODO: distinguish between fatal and workable errors

int new_debugger(int c_pid, char *file, Debugger **__dbg) {
    Debugger *dbg = (Debugger *)malloc(sizeof(Debugger));
    dbg->c_pid = c_pid;
    dbg->breakpoints = hashmap_new(sizeof(Breakpoint), 0, 0, 0, breakpoint_hash,
                                   breakpoint_cmp, NULL, NULL);
    int res = get_load_address(c_pid, &dbg->load_address);
    if (res) return res;
    res = init_dwarf(file, &dbg->dwarf_dbg);
    if (res) return res;
    res = init_unw(dbg);
    if (res) return res;
    *__dbg = dbg;
    return 0;
}

void free_debugger(Debugger *dbg) {
    dwarf_finish(dbg->dwarf_dbg);
    _UPT_destroy(dbg->context);
    unw_destroy_addr_space(dbg->as);
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
    if (waitpid(dbg->c_pid, status, options) == -1) return WAIT_PID_ERROR;

    if (debugee_terminated(*status)) return DEBUGEE_EXITED;

    return 0;
}

int wait_and_handle_signal(Debugger *dbg) {
    int status, options = 0, res;
    res = wait_for_signal(dbg, &status, options);
    if (res) return res;

    siginfo_t info;
    res = get_siginfo(dbg, &info);
    if (res) return res;

    switch (info.si_signo) {
    case SIGTRAP:
        res = handle_sigtrap(dbg, &info);
        break;
    default:
        res = -1;
        fprintf(stdout, "\nUnhandled signal: %x\n", info.si_code);
        break;
    }

    return res;
}

int debugee_terminated(int status) {
    if (WIFEXITED(status) || WIFSIGNALED(status)) {
        return 1;
    }
    return 0;
}

int init_unw(Debugger *dbg) {
    dbg->as = unw_create_addr_space(&_UPT_accessors, 0);
    if (!dbg->as) {
        return 1;
    }

    dbg->context = _UPT_create(dbg->c_pid);
    if (!dbg->context) {
        return 1;
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

        UWORD *pc;
        get_register(&regs, rip, &pc); // TODO: change io
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
        return PTRACE_ERROR;
    }
    return 0;
}

int ptrace_peek_data(Debugger *dbg, UWORD addr, UWORD *data) {
    errno = 0;
    *data = ptrace(PTRACE_PEEKDATA, dbg->c_pid, addr, NULL);
    if (*data == -1 && errno) {
        return PTRACE_ERROR;
    }
    return 0;
}

int run_debugger(Debugger *dbg) { // application
    if (dbg == NULL) {
        return 1;
    }

    int res;
    res = wait_and_handle_signal(dbg);
    if (res == WAIT_PID_ERROR) {
        fprintf(stderr, "Error %i on wait_for_signal: %s\n", errno,
                get_waitpid_err(errno)); // cmdline
        return res;
    } else if (res == PTRACE_ERROR) {
        fprintf(stderr, "Error %i on ptrace: %s\n", errno,
                get_ptrace_err(errno)); // cmdline
        return res;
    } else if (res) {
        fprintf(stdout, "Failed to start main loop: %i", res);
        return res;
    }

    COMMAND cmnd;
    Buffer *buffer = new_buffer(CMD_MAX_SIZE), *line = new_buffer(0);
    while (res != DEBUGEE_EXITED && cmnd != EXIT && poll_input(line)) {
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

            res = handle_command(dbg, cmnd, buffer);
            if (!res || res == DEBUGEE_EXITED) {
                fprintf(stdout, "\nOK\n"); // cmdline
            } else {
                fprintf(stdout, "\n!x!\n"); // cmdline
            }

            if (res == DEBUGEE_EXITED) break;
            reset_seek(buffer);
        }

        linenoiseFree(line->data);
        reset_seek(line);
    }
    fprintf(stdout, "Debugee terminated\n"); // cndline

    free(buffer->data);
    free(buffer);
    free(line);
    return 0;
}

int debug_continue(Debugger *dbg) {
    if (ptrace(PTRACE_CONT, dbg->c_pid, NULL, NULL) == -1) {
        return PTRACE_ERROR;
    }
    return 0;
}

int handle_command(Debugger *dbg, COMMAND cmnd, Buffer *buffer) {
    int res = 0;
    switch (cmnd) {
    case CONTINUE:
        res = step_over_breakpoint(dbg);
        if (res) return res;
        res = debug_continue(dbg);
        if (res) return res;
        res = wait_and_handle_signal(dbg);
        if (res) return res;
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
    case NEXT:
        res = step_in(dbg);
        break;
    case BACKTRACE:
        res = backtrace(dbg);
        break;
    case VAR:
        res = handle_var(dbg, buffer);
        break;
    case PEEK_DATA:
        res = handle_peek(dbg, buffer);
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
        if (*argEnd) return INVALID_COMMAND;
        break;
    case BREAKPOINT_ARG_LINENUM:
        char *file_name = argToken;
        argToken = next_token(buffer);
        arg = strtoll(argToken, &argEnd, 10);
        if (*argEnd) return INVALID_COMMAND;
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
        return INVALID_COMMAND;
    }
    arg += dbg->load_address;

    int res;
    Breakpoint *breakpoint;
    res = get_breakpoint_at_addr(dbg, arg, &breakpoint);
    if (res && res != KEY_NOT_EXISTS) return res;

    switch (action) {
    case ENABLE_BREAKPOINT:
        res = enable_breakpoint(dbg, breakpoint);
        if (res) return 1;
        res = save_breakpoint(dbg, breakpoint);
        break;
    case DISABLE_BREAKPOINT:
        res = disable_breakpoint(dbg, breakpoint);
        if (res) return 1;
        res = delete_breakpoint(dbg, breakpoint);
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
    breakpoint->temporary_enable = false;
    return breakpoint;
}

int copy_breakpoint(Breakpoint *dest, const Breakpoint *src) {
    if (src == NULL) return 1;
    dest->enabled = src->enabled;
    dest->mem_addr = src->mem_addr;
    dest->saved_data = src->saved_data;
    dest->temporary_enable = dest->temporary_enable;
}

// TODO: free allocated breakpoints at every point
int get_breakpoint_at_addr(Debugger *dbg, WORD memAddr,
                           Breakpoint **breakpoint) {
    *breakpoint = make_breakpoint(memAddr);
    if (!breakpoint) return 1;
    const Breakpoint *bp_in_map = hashmap_get(dbg->breakpoints, *breakpoint);
    if (copy_breakpoint(*breakpoint, bp_in_map)) {
        return KEY_NOT_EXISTS;
    }
    return 0;
}

int save_breakpoint(Debugger *dbg, Breakpoint *breakpoint) {
    hashmap_set(dbg->breakpoints, breakpoint);
    return 0;
}

int delete_breakpoint(Debugger *dbg, Breakpoint *breakpoint) {
    hashmap_delete(dbg->breakpoints, breakpoint);
    return 0;
}

int enable_breakpoint(Debugger *dbg, Breakpoint *breakpoint) {
    int res;

    UWORD data;
    res = ptrace_peek_data(dbg, breakpoint->mem_addr, &data);
    if (res) return res;

    breakpoint->saved_data = data;

    data = (data & ~0xff) | 0xcc;

    if (ptrace(PTRACE_POKEDATA, dbg->c_pid, breakpoint->mem_addr, data, NULL) ==
        -1) {
        return PTRACE_ERROR;
    }

    breakpoint->enabled = true;
    return 0;
}

int disable_breakpoint(Debugger *dbg, Breakpoint *breakpoint) {
    if (ptrace(PTRACE_POKEDATA, dbg->c_pid, breakpoint->mem_addr,
               breakpoint->saved_data, NULL) == -1) {
        return PTRACE_ERROR;
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
    res = get_regs_struct(dbg, &regs);
    if (res) return res;

    switch (action) {
    case READ_REGISTER:
        if (mode == REGISTER_ARG_ALL) {
            if (arg) {
                fprintf(stdout, "invalid arg %s\n", arg);
                return INVALID_COMMAND;
            }

            print_registers(&regs);
            return 0;
        }

        if (!arg) return INVALID_COMMAND;

        if (mode == REGISTER_ARG_ABBR) {
            reg = abbr_to_reg(arg);
        } else if (mode == REGISTER_ARG_DWARF) {
            int dwarfn = strtol(arg, &arg, 16);
            if (arg + 1 < buffer->data + buffer->rseek) {
                return INVALID_COMMAND;
            }
            reg = DW_to_reg(dwarfn);
        }
        if (reg == NO_SUCH_REGISTER) return 1;

        UWORD *p_reg;
        res = get_register(&regs, reg, &p_reg);
        if (res) return res;
        fprintf(stdout, "value :%llx\n", *p_reg); // cmdline

        return 0;
    case WRITE_REGISTER:
        if (!arg) return INVALID_COMMAND;

        if (mode == REGISTER_ARG_ABBR) {
            reg = abbr_to_reg(arg);
        } else if (mode == REGISTER_ARG_DWARF) {
            int dwarfn = strtol(arg, &arg, 10);
            if (arg + 1 < buffer->data + buffer->rseek) {
                return INVALID_COMMAND;
            }
            reg = DW_to_reg(dwarfn);
        }
        if (reg == NO_SUCH_REGISTER) return 1;

        arg = next_token(buffer);
        if (!arg) return INVALID_COMMAND;
        WORD value = strtoll(arg, &arg, 10);
        if (arg + 1 < buffer->data + buffer->rseek) {
            return INVALID_COMMAND;
        }

        set_reg_value(dbg, reg, value);

        return 0;
    }

    return INVALID_COMMAND;
}

int get_regs_struct(Debugger *dbg, regs_struct *regs) {
    if (ptrace(PTRACE_GETREGS, dbg->c_pid, NULL, regs) == -1) {
        return PTRACE_ERROR;
    }
    return 0;
}

int set_regs_struct(Debugger *dbg, regs_struct *regs) {
    if (ptrace(PTRACE_SETREGS, dbg->c_pid, NULL, regs) == -1) {
        return PTRACE_ERROR;
    }
    return 0;
}

int get_reg_value(Debugger *dbg, REGISTER reg, WORD *value) {
    int res;

    regs_struct regs;
    if ((res = get_regs_struct(dbg, &regs)) > 0) return res;

    UWORD *regl;
    res = get_register(&regs, reg, &regl);
    if (res) return NO_SUCH_REGISTER;

    *value = *regl;
    return 0;
}

int set_reg_value(Debugger *dbg, REGISTER reg, WORD value) {
    int res;

    regs_struct regs;
    if (res = get_regs_struct(dbg, &regs)) return res;

    UWORD *regl;
    res = get_register(&regs, reg, &regl);
    if (res) return NO_SUCH_REGISTER;

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
        return PTRACE_ERROR;
    }
    return 0;
}

int step_over_breakpoint(Debugger *dbg) {
    int res;
    regs_struct regs;
    res = get_regs_struct(dbg, &regs);
    if (res) return res;

    UWORD *program_counter;
    res = get_register(&regs, rip, &program_counter);
    if (res) return res;

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

    res = wait_and_handle_signal(dbg);
    if (res) return res;

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

            res = wait_and_handle_signal(dbg);
            if (res) return res;
        }
    }
    if (res) return res;
    return 0;
}

// =======================================

// =======================================
// NEXT
// =======================================

int step_in(Debugger *dbg) {
    int res = 0;

    WORD pc;
    res = get_reg_value(dbg, rip, &pc);
    if (res) return res;

    Dwarf_Die subprog_die = 0, cu_die = 0;
    Dwarf_Error err;

    res =
        get_cu_from_addr(pc - dbg->load_address, dbg->dwarf_dbg, &cu_die, &err);
    if (res == DW_DLV_ERROR) return 1; // fatal
    if (cu_die == 0) return 1;         // couldnt find

    Line_Iterator line_iterator;
    res = get_line_iterator(dbg->dwarf_dbg, cu_die, &line_iterator, &err);
    if (res != DW_DLV_OK) {
        dwarf_dealloc_die(cu_die);
        return 1;
    }

    attach_cu_die(&line_iterator, cu_die);

    res = get_sub_prog_die_in_die_from_addr(
        pc - dbg->load_address, dbg->dwarf_dbg, cu_die, &subprog_die, &err);
    if (res == DW_DLV_ERROR) {
        free_line_iterator(&line_iterator);
        return 1; // Fatal
    } else if (subprog_die == 0) {
        return 1; // couldnt find
    }

    Dwarf_Addr low_pc, high_pc;
    res = get_low_high_pc_from_die(dbg->dwarf_dbg, subprog_die, &low_pc,
                                   &high_pc, &err);
    dwarf_dealloc_die(subprog_die);
    if (res != DW_DLV_OK) {
        free_line_iterator(&line_iterator);
        return 1;
    }

    Dwarf_Signed line_index;
    res = search_addr_in_lines(&line_iterator, low_pc, &line_index, &err);
    if (res) {
        free_line_iterator(&line_iterator);
        return 1;
    }

    Breakpoint *breakpoint;

    Dwarf_Unsigned line_addr;
    for (Dwarf_Unsigned i = line_index; i < line_iterator.line_count; i++) {
        res = dwarf_lineaddr(line_iterator.dw_lines[i], &line_addr, &err);
        if (res != DW_DLV_OK) {
            free_line_iterator(&line_iterator);

            return 1;
        } else if (line_addr >= high_pc) {
            break;
        }

        // Absolute pc from this point
        line_addr += dbg->load_address;

        res = get_breakpoint_at_addr(dbg, line_addr, &breakpoint);
        if (res && res != KEY_NOT_EXISTS) {
            free_line_iterator(&line_iterator);

            return res;
        }

        if (!res && breakpoint->enabled) {
            free(breakpoint);

            continue;
        }

        breakpoint = make_breakpoint(line_addr);
        if (res == KEY_NOT_EXISTS || !breakpoint->enabled) {
            breakpoint->temporary_enable = true;
        }

        res = enable_breakpoint(dbg, breakpoint);
        if (res) {
            free(breakpoint);
            free_line_iterator(&line_iterator);

            return res;
        }

        res = save_breakpoint(dbg, breakpoint);
        free(breakpoint);

        if (res) {
            free_line_iterator(&line_iterator);

            return res;
        }
    }

    res = step_out(dbg);
    if (res) return res;

    for (Dwarf_Unsigned i = line_index; i < line_iterator.line_count; i++) {
        res = dwarf_lineaddr(line_iterator.dw_lines[i], &line_addr, &err);
        if (res != DW_DLV_OK) {
            free_line_iterator(&line_iterator);
            return 1;
        } else if (line_addr >= high_pc) {
            break;
        }

        line_addr += dbg->load_address;

        res = get_breakpoint_at_addr(dbg, line_addr, &breakpoint);
        if (res && res != KEY_NOT_EXISTS) return res;

        if (res == KEY_NOT_EXISTS || !breakpoint->enabled ||
            !breakpoint->temporary_enable) {
            free(breakpoint);
            continue;
        }

        res = disable_breakpoint(dbg, breakpoint);
        free(breakpoint);
        if (res) {
            return res;
        }
    }

    free_line_iterator(&line_iterator);
    return 0;
}

// =======================================

// =======================================
// FINISH
// =======================================

// TODO: consider fpo and use libunwind here
int step_out(Debugger *dbg) {
    int res;

    unw_cursor_t cursor;
    unw_word_t frame_base;
    if (unw_init_remote(&cursor, dbg->as, dbg->context) < 0) return 1;

    res = unw_step(&cursor);
    if (res < 0) return 1;
    if (res == 0) return 0;

    res = unw_get_reg(&cursor, UNW_REG_SP, &frame_base);
    if (res) return 1;

    UWORD return_addr;
    res = ptrace_peek_data(dbg, frame_base - 8, &return_addr);
    if (res) return res;

    Breakpoint *breakpoint;
    res = get_breakpoint_at_addr(dbg, return_addr, &breakpoint);
    if (res && res != KEY_NOT_EXISTS) return res;

    int temp = res == KEY_NOT_EXISTS || !breakpoint->enabled;

    res = enable_breakpoint(dbg, breakpoint);
    if (res) return res;

    res = step_over_breakpoint(dbg);
    if (res) {
        free(breakpoint);
        return res;
    }

    res = debug_continue(dbg);
    if (res) {
        free(breakpoint);
        return res;
    }

    res = wait_and_handle_signal(dbg);
    if (res) {
        free(breakpoint);
        return res;
    }

    if (temp) {
        res = disable_breakpoint(dbg, breakpoint);
        if (res) {
            free(breakpoint);
            return res;
        }
    }

    free(breakpoint);
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

        char *func_name;
        res = get_subprog_name(dbg, pc, &func_name);
        if (res) return res;

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

int get_subprog_name(Debugger *dbg, WORD addr, char **func_name) {
    int res;

    Dwarf_Die sub_prog_die = 0;
    Dwarf_Error error;
    res = get_sub_prog_die_from_addr(addr - dbg->load_address, dbg->dwarf_dbg,
                                     &sub_prog_die, &error);

    if (res == DW_DLV_ERROR) {
        fprintf(stderr, "error: %s\n", dwarf_errmsg(error));
        dwarf_dealloc_error(dbg->dwarf_dbg, error);
        return 1;
    }

    res = get_sub_program_name(dbg->dwarf_dbg, sub_prog_die, func_name, &error);

    dwarf_dealloc_die(sub_prog_die);
    if (res == DW_DLV_ERROR) {
        dwarf_dealloc_error(dbg->dwarf_dbg, error);
        return 1;
    } else if (res == DW_DLV_NO_ENTRY) {
        return 1;
    }

    return 0;
}

// =======================================

// =======================================
// Backtrace
// =======================================

int backtrace(Debugger *dbg) {
    unw_cursor_t cursor;
    if (unw_init_remote(&cursor, dbg->as, dbg->context) < 0) {
        return 1;
    }

    int res;

    do {
        unw_word_t ip, sp, offset;
        char sym[4096];

        res = get_preserved_registers(&cursor, &ip, &sp);
        if (res) {
            fprintf(stdout, "error reading caller registers: %i\n", -res);
            return 1;
        }

        res = unw_get_proc_name(&cursor, sym, sizeof(sym), &offset);
        if (res < 0) {
            if (res == -UNW_ENOINFO) {
                res = 0;
                continue;
            }
            fprintf(stdout, "error reading name: %i\n", -res);
            return 1;
        } // TODO: Error handling

        fprintf(stdout, "ip:%llx sp:%llx\n", ip, sp);
        fprintf(stdout, "(%s+0x%lx)\n", sym, offset);
    } while ((res = unw_step(&cursor)) > 0);

    if (res < 0) {
        fprintf(stdout, "error reading ip: %i\n", -res);
        return 1;
    } // TODO: Error handling

    return 0;
}

int get_preserved_registers(unw_cursor_t *cursor, unw_word_t *ip,
                            unw_word_t *sp) {
    int res = 0;

    res = unw_get_reg(cursor, UNW_REG_IP, ip);
    if (res < 0) {
        return 1;
    } // TODO: Error handling

    res = unw_get_reg(cursor, UNW_REG_SP, sp);
    if (res < 0) {
        return 1;
    } // TODO: Error handling

    return 0;
}

// =======================================

// =======================================
// Var
// =======================================

int handle_var(Debugger *dbg, Buffer *buffer) {
    char *arg = next_token(buffer);
    if (!arg) return 1;

    VAR_OPTIONS mode = match_var_option(arg);

    switch (mode) {
    default:
        return 1;
    case GET_VAR_ADDR_OPTION:
    case GET_VAR_VALUE_OPTION:
        break;

    case SET_VAR_VALUE_OPTION:
        return 1; // TODO: implement
    }

    arg = next_token(buffer);
    if (!arg) return 1;

    VAR_OPTIONS op = match_var_option(arg);

    switch (op) {
    case PARAM_VAR_OPTION:
        return handle_param_var(dbg, mode, 0);

    case LOCAL_VAR_OPTION:
        arg = next_token(buffer);
        return handle_get_local_var(dbg, mode, arg);

    default:
        // is a variable name so do that stuff
        // TODO: implement
        return 1;
    }

    return 0;
}

int handle_param_var(Debugger *dbg, VAR_OPTIONS var_option, UWORD data) {
    int res;

    Loc_Info loc_info = {.c_pid = dbg->c_pid,
                         .load_address = dbg->load_address,
                         .frame_base = 0,
                         .h_initial = 0};
    res = init_loc_info(dbg, &loc_info);
    if (res) return res;

    UWORD pc;
    res = get_reg_value(dbg, rip, &pc);
    if (res) return res;

    Dwarf_Die sub_prog_die, fparam = NULL;
    Dwarf_Error error;
    res = get_sub_prog_die_from_addr(
        /*loc_info.callee_regs_cur_value.rip*/ pc - dbg->load_address,
        dbg->dwarf_dbg, &sub_prog_die, &error);
    if (res == DW_DLV_ERROR) {
        dwarf_dealloc_error(dbg->dwarf_dbg, error);
        return 1;
    }

    res = get_var_location(dbg, loc_info, sub_prog_die, DW_AT_frame_base,
                           &loc_info.frame_base, &error);
    if (res) return res;

    char *func_name;
    res = dwarf_diename(sub_prog_die, &func_name, &error);
    if (res) {
        dwarf_dealloc_die(sub_prog_die);
        return 1;
    }
    fprintf(stdout, "Printing formal parameter addresses of func: %s\n",
            func_name);

    int i = -1;
    char *fparam_name;
    while ((res = get_formal_params(dbg->dwarf_dbg, sub_prog_die, &fparam,
                                    &error)) == DW_DLV_OK) {
        i++;

        res = dwarf_diename(fparam, &fparam_name, &error);
        if (res == DW_DLV_ERROR) {
            dwarf_dealloc_die(sub_prog_die);
            dwarf_dealloc_die(fparam);
            return 1;
        } else if (res == DW_DLV_NO_ENTRY) {
            fprintf(stdout, "Could not fetch a name for  parameter %i\n", i);
        } else {
            fprintf(stdout, "Formal parameter %i: %s\n", i, fparam_name);
        }

        UWORD var_addr;
        res = get_var_location(dbg, loc_info, fparam, DW_AT_location, &var_addr,
                               &error);
        if (res) {
            fprintf(stdout, "Could not fetch the location of parameter %i\n",
                    i);
            continue;
        }

        char var_val_s[256] = {};
        switch (var_option) {
        case GET_VAR_ADDR_OPTION:
            fprintf(stdout, "Evaluated address: %llx\n", var_addr);
            break;
        case GET_VAR_VALUE_OPTION:
            res = sprint_variable_die_val(dbg, fparam, var_addr, var_val_s,
                                          &error);
            if (res) {
                fprintf(stdout, "Could not fetch the value of parameter %i\n",
                        i);
                continue;
            }
            fprintf(stdout, "Evaluated value: %s\n", var_val_s);
            break;
        default:
            dwarf_dealloc_die(sub_prog_die);
            if (fparam != NULL) dwarf_dealloc_die(fparam);
            return 1;
        }
    }

    dwarf_dealloc_die(sub_prog_die);
    if (fparam != NULL) dwarf_dealloc_die(fparam);
    if (res == DW_DLV_ERROR) return 1;
    return 0;
}

int init_loc_info(Debugger *dbg, Loc_Info *loc_info) {
    int res;
    res = get_regs_struct(dbg, &loc_info->callee_regs_cur_value);
    if (res) return res;

    unw_cursor_t cursor;
    if (unw_init_remote(&cursor, dbg->as, dbg->context) < 0) return 1;

    res = unw_step(&cursor);
    if (res < 0) return 1;
    if (res == 0) return 0;

    res = get_preserved_registers(&cursor, &loc_info->caller_ip,
                                  &loc_info->caller_sp);
    if (res) return 1;

    return 0;
}

int get_var_location(Debugger *dbg, Loc_Info loc_info, Dwarf_Die var_die,
                     Dwarf_Half loc_attr, UWORD *var_addr, Dwarf_Error *error) {
    int res;
    Loc_Expr loc_expr = {.loc_info = loc_info};
    if (loc_info.h_initial) {
        push_expr_tkn(&loc_expr, (Value){.u = loc_info.h_initial});
    }

    Dwarf_Attribute location_attr = 0;
    res = dwarf_attr(var_die, loc_attr, &location_attr, error);
    if (res != DW_DLV_OK) return 1;

    // Getting loclist_head

    Dwarf_Unsigned lcount = 0;
    Dwarf_Loc_Head_c loclist_head = 0;
    res = dwarf_get_loclist_c(location_attr, &loclist_head, &lcount, error);
    if (res != DW_DLV_OK) {
        dwarf_dealloc_attribute(location_attr);
        dwarf_dealloc_loc_head_c(loclist_head);
        return 1;
    }

    // Getting locdesc_entry

    Dwarf_Small loclist_lkind = 0;
    Dwarf_Small lle_value = 0;
    Dwarf_Unsigned rawval1 = 0;
    Dwarf_Unsigned rawval2 = 0;
    Dwarf_Bool debug_addr_unavailable = false;
    Dwarf_Addr lopc = 0;
    Dwarf_Addr hipc = 0;
    Dwarf_Unsigned loclist_expr_op_count = 0;
    Dwarf_Locdesc_c locdesc_entry = 0;
    Dwarf_Unsigned expression_offset = 0;
    Dwarf_Unsigned locdesc_offset = 0;

    Dwarf_Unsigned i = 0;
    for (i = 0; i < lcount; ++i) {
        res = dwarf_get_locdesc_entry_d(
            loclist_head, i, &lle_value, &rawval1, &rawval2,
            &debug_addr_unavailable, &lopc, &hipc, &loclist_expr_op_count,
            &locdesc_entry, &loclist_lkind, &expression_offset, &locdesc_offset,
            error);
        if (res != DW_DLV_OK) {
            dwarf_dealloc_attribute(location_attr);
            dwarf_dealloc_loc_head_c(loclist_head);
            return 1;
        }

        if (lopc <=
                loc_info.callee_regs_cur_value.rip - loc_info.load_address &&
            hipc >=
                loc_info.callee_regs_cur_value.rip - loc_info.load_address) {
            break;
        }
    }

    // Evaluating loc_expr
    res = evaluate_expr(&loc_expr, loclist_expr_op_count, locdesc_entry, 0,
                        var_addr, error);
    if (res) {
        return 1;
    }

    dwarf_dealloc_attribute(location_attr);
    dwarf_dealloc_loc_head_c(loclist_head);
    return 0;
}

int evaluate_expr(Loc_Expr *loc_expr, Dwarf_Unsigned loclist_expr_op_count,
                  Dwarf_Locdesc_c locdesc_entry, Dwarf_Unsigned index,
                  UWORD *result, Dwarf_Error *error) {
    int res;

    for (Dwarf_Unsigned i = index; i < loclist_expr_op_count;
         /*increment in loop*/) {
        Dwarf_Small op = 0;
        Dwarf_Unsigned opd1 = 0;
        Dwarf_Unsigned opd2 = 0;
        Dwarf_Unsigned opd3 = 0;
        Dwarf_Unsigned offsetforbranch = 0;

        res = dwarf_get_location_op_value_c(locdesc_entry, i, &op, &opd1, &opd2,
                                            &opd3, &offsetforbranch, error);
        if (res != DW_DLV_OK) {
            if (res == DW_DLV_NO_ENTRY) {
                fprintf(stdout, "No entry\n");
            } else {
                fprintf(stdout, "Err: %s\n", dwarf_errmsg(*error));
            }
            return 1;
        }

        int16_t skip_bytes = 0;
        res = parse_opcode(loc_expr, op, opd1, opd2, opd3, &skip_bytes, NULL);
        switch (res) {
        case NEXT_OP:
            break;
        case PARSE_PIECE:
            return 1; // TODO: havent implemented;
        default:
            if (res < 0) {
                break;
            }
            fprintf(stdout, "Err %i\n", res);
            return 1;
        }

        if (skip_bytes == 0) {
            i++;
            continue;
        } else if (skip_bytes > 0) {
            Dwarf_Unsigned new_offset = offsetforbranch;
            while (new_offset - (offsetforbranch + 2) < skip_bytes &&
                   i + 1 < loclist_expr_op_count) {
                i++;
                res = dwarf_get_location_op_value_c(locdesc_entry, i, &op,
                                                    &opd1, &opd2, &opd3,
                                                    &new_offset, error);
                if (res != DW_DLV_OK) {
                    return 1;
                }
            }
        } else {
            Dwarf_Unsigned new_offset = offsetforbranch;
            while (new_offset - (offsetforbranch + 2) > skip_bytes && i > 0) {
                i--;
                res = dwarf_get_location_op_value_c(locdesc_entry, i, &op,
                                                    &opd1, &opd2, &opd3,
                                                    &new_offset, error);
                if (res != DW_DLV_OK) {
                    return 1;
                }
            }
        }
    }

    Value top;
    res = pop_stack_top(loc_expr, &top);
    if (res) {
        fprintf(stdout, "Err ::%i\n", res);
        return 1;
    }

    switch (res) {
    case RESULT_REGISTER:
        UWORD *p_reg = 0;
        res = get_register(&loc_expr->loc_info.callee_regs_cur_value, top.u,
                           &p_reg);
        if (res) {
            fprintf(stdout, "No reg!\n");
            return 1;
        }

        *result = *p_reg;
        return 0;

    case NEXT_OP:
    case RESULT_VALUE:
    default:
        *result = top.u;
        return 0;
    }
}

int sprint_variable_die_val(Debugger *dbg, Dwarf_Die var_die, UWORD var_addr,
                            char print_output[], Dwarf_Error *error) {
    int res;

    Dwarf_Off type_offset;
    Dwarf_Bool is_info;
    Dwarf_Half type_tag;
    Dwarf_Die type_die = 0, prev_die = var_die;

    while (true) {
        // get type offset of previous die
        res = dwarf_dietype_offset(prev_die, &type_offset, &is_info, error);
        if (prev_die != var_die) {
            dwarf_dealloc_die(prev_die); // free previous die
        }
        if (res != DW_DLV_OK) return res;

        // get type of previous die
        res = dwarf_offdie_b(dbg->dwarf_dbg, type_offset, is_info, &type_die,
                             error);
        if (res != DW_DLV_OK) return res;

        // get the tag of the type
        res = dwarf_tag(type_die, &type_tag, error);
        if (res != DW_DLV_OK) {
            dwarf_dealloc_die(type_die);
            return res;
        }
        prev_die = type_die;

        switch (type_tag) {
        default:
            // Unimplemented or invalid
            dwarf_dealloc_die(type_die);
            return 1;

        case DW_TAG_base_type: // Base case
            return sprint_var_die_given_base(dbg, var_die, var_addr, type_die,
                                             print_output, error);

        case DW_TAG_structure_type:
            return sprint_var_die_given_struct_type(
                dbg, var_die, var_addr, type_die, print_output, 0, error);

        case DW_TAG_array_type:
        case DW_TAG_class_type:
        case DW_TAG_union_type:
            // Not implemented
            // TODO: Recursive call here
            dwarf_dealloc_die(type_die);
            return 1;

        case DW_TAG_atomic_type:
        case DW_TAG_const_type:
        case DW_TAG_immutable_type:
        case DW_TAG_packed_type:
        case DW_TAG_pointer_type:
        case DW_TAG_reference_type:
        case DW_TAG_restrict_type:
        case DW_TAG_rvalue_reference_type:
        case DW_TAG_shared_type:
        case DW_TAG_volatile_type:
        case DW_TAG_typedef:
            // These do not affect the value in any way
            continue;
        }
    }
}

int sprint_var_die_given_base(Debugger *dbg, Dwarf_Die var_die, UWORD var_addr,
                              Dwarf_Die type_die, char print_output[],
                              Dwarf_Error *error) {
    int res;
    Dwarf_Unsigned byte_size;
    Dwarf_Attribute at_encoding;
    Dwarf_Signed encoding;

    _var_type data;
    res = ptrace_peek_data(dbg, var_addr, &data._type_integral);
    if (res) return res;

    res = dwarf_attr(type_die, DW_AT_encoding, &at_encoding, error);
    if (res) return res;

    res = dwarf_formsdata(at_encoding, &encoding, error);
    dwarf_dealloc_attribute(at_encoding);
    if (res) return res;

    res = dwarf_bytesize(type_die, &byte_size, error);
    if (res == DW_DLV_ERROR) {
        return 1;
    } else if (res == DW_DLV_NO_ENTRY) {
        byte_size = 8;
    } else if (byte_size > 8) {
        return 1; // TODO: implement
    } else if (byte_size < 8) {
        data._type_integral &= (1ll << (8 * byte_size)) - 1;
    }

    switch (encoding) {
    case DW_ATE_boolean:
        if (data._type_integral) {
            sprintf(print_output, "true");
        } else {
            sprintf(print_output, "false");
        }
        break;

    case DW_ATE_address:
        sprintf(print_output, "%llx", data._type_integral);
        break;

    case DW_ATE_unsigned:
    case DW_ATE_unsigned_char:
        sprintf(print_output, "%llu", data._type_integral);
        break;

    case DW_ATE_signed:
    case DW_ATE_signed_char:
        sprintf(print_output, "%lli", data._type_integral);
        break;

    case DW_ATE_float:
        sprintf(print_output, "%lf", data._type_floating);
        break;

    default:
        return 1;
    }

    return 0;
}

int sprint_var_die_given_struct_type(Debugger *dbg, Dwarf_Die var_die,
                                     UWORD var_addr, Dwarf_Die struct_type_die,
                                     char print_output[],
                                     int pretty_print_level,
                                     Dwarf_Error *error) {
    int res, cur_offset = 0, i = -1;
    char *member_val, *member_name;
    Dwarf_Die member_die = NULL;

    Loc_Info loc_info = {.c_pid = dbg->c_pid,
                         .load_address = dbg->load_address,
                         .frame_base = 0,
                         .h_initial = 1,
                         .inital = var_addr};
    res = init_loc_info(dbg, &loc_info);
    if (res) return res;

    cur_offset += sprintf(print_output + cur_offset, "{");
    while ((res = get_data_members(dbg->dwarf_dbg, struct_type_die, &member_die,
                                   error)) == DW_DLV_OK) {
        i++;
        if (i > 0) {
            cur_offset += sprintf(print_output + cur_offset, ", ");
        }

        res = dwarf_diename(member_die, &member_name, error);
        if (res == DW_DLV_ERROR) {
            dwarf_dealloc_die(member_die);
            return 1;
        } else if (res == DW_DLV_NO_ENTRY) {
            cur_offset += sprintf(print_output + cur_offset, "[%i]--: ", i);
        } else {
            cur_offset +=
                sprintf(print_output + cur_offset, "[%i]%s: ", i, member_name);
        }

        UWORD member_addr;
        Dwarf_Attribute data_member_location;
        dwarf_attr(member_die, DW_AT_data_member_location,
                   &data_member_location, error);
        if (res == DW_DLV_ERROR) {
            return res;
        } else if (res == DW_DLV_NO_ENTRY) {
            member_addr = var_addr;
        } else {
            Dwarf_Half form;
            res = dwarf_whatform(data_member_location, &form, error);
            if (res) {
                dwarf_dealloc_attribute(data_member_location);
                return res;
            }

            switch (form) {

            // integer constant forms
            case DW_FORM_data1:
            case DW_FORM_data2:
            case DW_FORM_data4:
            case DW_FORM_data8:
            case DW_FORM_sdata:
            case DW_FORM_udata:
                res = dwarf_formsdata(data_member_location, &member_addr,
                                      error); // get offset
                if (res) {
                    dwarf_dealloc_attribute(data_member_location);
                    return res;
                }
                member_addr += var_addr; // add to base
                break;

            // location expression forms
            case DW_FORM_exprloc:
            case DW_FORM_loclistx:
            case DW_FORM_sec_offset:
                get_var_location(dbg, loc_info, member_die, DW_AT_data_location,
                                 &member_addr, error);
                break;

            case DW_FORM_data16: // How to interpret? Will it be encountered?
            default:             // Invalid
                dwarf_dealloc_attribute(data_member_location);
                return res;
            }
        }

        res = sprint_variable_die_val(dbg, member_die, member_addr,
                                      print_output + cur_offset, error);
        if (res) return res;
        cur_offset += strlen(print_output + cur_offset);
    }
    cur_offset += sprintf(print_output + cur_offset, "}");

    dwarf_dealloc_die(member_die);
    return 0;
}

int handle_get_local_var(Debugger *dbg, VAR_OPTIONS mode, char *var_name) {
    int res;

    Loc_Info loc_info = {.c_pid = dbg->c_pid,
                         .load_address = dbg->load_address,
                         .frame_base = 0,
                         .h_initial = 0};
    res = init_loc_info(dbg, &loc_info);
    if (res) return res;

    UWORD pc;
    res = get_reg_value(dbg, rip, &pc);
    if (res) return res;

    Dwarf_Die sub_prog_die;
    Dwarf_Error error;
    res = get_sub_prog_die_from_addr(pc - dbg->load_address, dbg->dwarf_dbg,
                                     &sub_prog_die, &error);
    if (res == DW_DLV_ERROR) {
        dwarf_dealloc_error(dbg->dwarf_dbg, error);
        return 1;
    }

    res = get_var_location(dbg, loc_info, sub_prog_die, DW_AT_frame_base,
                           &loc_info.frame_base, &error);
    if (res) return res;

    Dwarf_Die var_die = NULL;
    res = get_local_var_from_name(dbg->dwarf_dbg, sub_prog_die, var_name,
                                  &var_die, pc - dbg->load_address, &error);
    dwarf_dealloc_die(sub_prog_die);
    if (res != DW_DLV_OK) {
        return res;
    }

    Dwarf_Attribute at_decl_line;
    Dwarf_Signed decl_line;
    res = dwarf_attr(var_die, DW_AT_decl_line, &at_decl_line, &error);
    if (res == DW_DLV_ERROR) {
        dwarf_dealloc_die(var_die);
        return res;
    } else if (res == DW_DLV_NO_ENTRY) {
        decl_line = 0;
    } else {
        res = dwarf_formsdata(at_decl_line, &decl_line, &error);
        dwarf_dealloc_attribute(at_decl_line);
        if (res) {
            dwarf_dealloc_die(var_die);
            return res;
        }
    }

    UWORD var_addr;
    res = get_var_location(dbg, loc_info, var_die, DW_AT_location, &var_addr,
                           &error);
    if (res) {
        dwarf_dealloc_die(var_die);
        return res;
    }

    char var_val_s[256] = {};
    res = sprint_variable_die_val(dbg, var_die, var_addr, var_val_s, &error);
    if (res) {
        dwarf_dealloc_die(var_die);
        return res;
    }

    fprintf(stdout, "%s (declared at line %lli): %s\n", var_name, decl_line,
            var_val_s);
    if (var_die != NULL) dwarf_dealloc_die(var_die);
    return 0;
}

// =======================================

// =======================================
// Peek
// =======================================

int handle_peek(Debugger *dbg, Buffer *buffer) {
    char *arg = next_token(buffer), *argEnd;
    if (!arg) return 1;

    UWORD addr = strtoll(arg, &argEnd, 16);
    if (*argEnd) return INVALID_COMMAND;

    int res;
    UWORD data;
    res = ptrace_peek_data(dbg, addr, &data);
    if (res) return 1;

    fprintf(stdout, "Data: %llx\n", data);
    return 0;
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