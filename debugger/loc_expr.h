#ifndef DW_LOC_EXPR
#define DW_LOC_EXPR

#include "registers.h"

#include <libunwind-ptrace.h>

#include "libdwarf-2/dwarf.h"
#include "libdwarf-2/libdwarf.h"

#define EXPR_STACK_SIZE 256

// TODO: move to an enum in a file for errors
typedef enum Expr_Errors {
    EXPR_INDEX_OOB = 0xff,
    EXPR_STACK_UNDERFLOW,
    EXPR_STACK_OVERFLOW,
    UNIMPLEMENTED_OP_CODE,
    INVALID_OPCODE
} Expr_Errors;

typedef enum DW_Expr_Eval_Action {
    RESULT_REGISTER = -1,
    RESULT_ADDRESS = -2,
    RESULT_VALUE = -3,
    RESULT_IMPLICIT_VALUE = -4,
    RESULT_IMPLICIT_POINTER = -5,

    NEXT_OP = 0,

    PARSE_PIECE,
    SKIP_OFFSET // TODO: remove
} DW_Expr_Action;

typedef struct Loc_Info {
    // Debugee PID for ptrace access
    UWORD c_pid;

    // Load address
    UWORD load_address;

    // Current value of callee's registers
    regs_struct callee_regs_cur_value;

    // Value of registers at callee function entry
    regs_struct callee_regs_entry_value;

    // Value of the stack pointer of the caller
    unw_word_t caller_sp;

    // Value of the instruction pointer of the caller
    unw_word_t caller_ip;

    // Frame base of callee function
    UWORD frame_base;

    // address of the object2 currently being evaluated
    UWORD obj_addr;

    // Initial stack entry
    UWORD inital;

    // Flag for if initial entry exists
    char h_initial;
} Loc_Info;

typedef union Value {
    UWORD u;
    WORD s;
} Value;

typedef struct Loc_Expr {
    Loc_Info loc_info;

    Value op_stack[EXPR_STACK_SIZE];
    int stack_top;
} Loc_Expr;

static inline int push_expr_tkn(Loc_Expr *loc_expr, Value tkn) {
    if (loc_expr->stack_top == EXPR_STACK_SIZE) return EXPR_STACK_OVERFLOW;

    loc_expr->op_stack[loc_expr->stack_top++] = tkn;
    return 0;
}

static inline int pop_stack_top(Loc_Expr *loc_expr, Value *data) {
    if (loc_expr->stack_top == 0) return EXPR_STACK_UNDERFLOW;

    if (data) {
        *data = loc_expr->op_stack[loc_expr->stack_top - 1];
    }

    loc_expr->stack_top--;
    return 0;
}

int parse_opcode(Loc_Expr *loc_expr, int op, UWORD op1, UWORD op2, UWORD op3,
                 int16_t *bytes, WORD *offset);

int parse_op_addr(Loc_Expr *loc_expr, UWORD op1);

int parse_op_deref(Loc_Expr *loc_expr, UWORD size);

int parse_op_div(Loc_Expr *loc_expr);

int parse_op_dup(Loc_Expr *loc_expr);

int parse_op_pick(Loc_Expr *loc_expr, UWORD op1);

int parse_op_swap(Loc_Expr *loc_expr);

int parse_op_rot(Loc_Expr *loc_expr);

int parse_op_abs(Loc_Expr *loc_expr);

int parse_op_plus_uconst(Loc_Expr *loc_expr, UWORD op1);

int parse_op_plus_div(Loc_Expr *loc_expr);

int parse_op_shl(Loc_Expr *loc_expr);

int parse_op_shr(Loc_Expr *loc_expr);

int parse_op_shra(Loc_Expr *loc_expr);

int parse_op_bra(Loc_Expr *loc_expr, UWORD op1, int16_t *bytes);

static int __ptrace_peek_data(UWORD c_pid, UWORD addr, UWORD *data);

#endif