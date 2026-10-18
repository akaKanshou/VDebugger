#include "loc_expr.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/ptrace.h>

int parse_opcode(Loc_Expr *loc_expr, int op, UWORD op1, UWORD op2, UWORD op3,
                 int16_t *bytes, WORD * /*offset*/) { // TODO: remove offset
    int res;
    Value b, c;
    UWORD *p_reg;

    /*
    Snippet taken from https://github.com/aclements/libelfin
    */
#define UBINOP(op)                                                             \
    do {                                                                       \
        if ((res = pop_stack_top(loc_expr, &b)) ||                             \
            (res = pop_stack_top(loc_expr, &c))) {                             \
            return res;                                                        \
        }                                                                      \
        b.u = c.u op b.u;                                                      \
        push_expr_tkn(loc_expr, b);                                            \
    } while (0)

#define SBINOP(op)                                                             \
    do {                                                                       \
        if ((res = pop_stack_top(loc_expr, &b)) ||                             \
            (res = pop_stack_top(loc_expr, &c))) {                             \
            return res;                                                        \
        }                                                                      \
        if (c.s op b.s) {                                                      \
            b.u = 1;                                                           \
        } else {                                                               \
            b.u = 0;                                                           \
        }                                                                      \
        push_expr_tkn(loc_expr, b);                                            \
    } while (0)

    switch (op) {
    case DW_OP_addr:
        return parse_op_addr(loc_expr, op1);

    case DW_OP_deref:
        return parse_op_deref(loc_expr, sizeof(UWORD));
    case DW_OP_deref_size:

    case DW_OP_const1u:
    case DW_OP_const1s:
    case DW_OP_const2u:
    case DW_OP_const2s:
    case DW_OP_const4u:
    case DW_OP_const4s:
    case DW_OP_const8u:
    case DW_OP_const8s:
    case DW_OP_constu:
    case DW_OP_consts:
        b.u = op1;
        return push_expr_tkn(loc_expr, b);

    case DW_OP_dup:
        return parse_op_dup(loc_expr);

    case DW_OP_drop:
        return pop_stack_top(loc_expr, &b);

    case DW_OP_over:
        return parse_op_pick(loc_expr, 1);

    case DW_OP_pick:
        return parse_op_pick(loc_expr, op1);

    case DW_OP_swap:
        return parse_op_swap(loc_expr);

    case DW_OP_rot:
        return parse_op_rot(loc_expr);

    case DW_OP_xderef:
    case DW_OP_xderef_size:
    case DW_OP_xderef_type: /* DWARF5 */
        return UNIMPLEMENTED_OP_CODE;

    case DW_OP_abs:
        return parse_op_abs(loc_expr);

    case DW_OP_and:
        UBINOP(&);
        return 0;

    case DW_OP_div:
        return parse_op_div(loc_expr);

    case DW_OP_minus:
        UBINOP(-);
        return 0;

    case DW_OP_mod:
        UBINOP(%);
        return 0;

    case DW_OP_mul:
        UBINOP(*);
        return 0;

    case DW_OP_neg:
        res = pop_stack_top(loc_expr, &b);
        if (res) return res;
        b.u = -b.s;
        return push_expr_tkn(loc_expr, b);

    case DW_OP_not:
        res = pop_stack_top(loc_expr, &b);
        if (res) return res;
        b.u = ~b.u;
        return push_expr_tkn(loc_expr, b);

    case DW_OP_or:
        UBINOP(|);
        return 0;

    case DW_OP_plus:
        UBINOP(+);
        return 0;

    case DW_OP_plus_uconst:
        return parse_op_plus_uconst(loc_expr, op1);

    case DW_OP_shl:
        return parse_op_shl(loc_expr);

    case DW_OP_shr:
        return parse_op_shr(loc_expr);

    case DW_OP_shra:
        return parse_op_shra(loc_expr);

    case DW_OP_xor:
        UBINOP(^);
        return 0;

    case DW_OP_skip:
        *bytes = op1;
        return 0;

    case DW_OP_bra:
        return parse_op_bra(loc_expr, op1, bytes);

    case DW_OP_eq:
        SBINOP(==);
        return 0;

    case DW_OP_ge:
        SBINOP(>=);
        return 0;

    case DW_OP_gt:
        SBINOP(>);
        return 0;

    case DW_OP_le:
        SBINOP(<=);
        return 0;

    case DW_OP_lt:
        SBINOP(<);
        return 0;

    case DW_OP_ne:
        SBINOP(!=);
        return 0;

    case DW_OP_lit0:
    case DW_OP_lit1:
    case DW_OP_lit2:
    case DW_OP_lit3:
    case DW_OP_lit4:
    case DW_OP_lit5:
    case DW_OP_lit6:
    case DW_OP_lit7:
    case DW_OP_lit8:
    case DW_OP_lit9:
    case DW_OP_lit10:
    case DW_OP_lit11:
    case DW_OP_lit12:
    case DW_OP_lit13:
    case DW_OP_lit14:
    case DW_OP_lit15:
    case DW_OP_lit16:
    case DW_OP_lit17:
    case DW_OP_lit18:
    case DW_OP_lit19:
    case DW_OP_lit20:
    case DW_OP_lit21:
    case DW_OP_lit22:
    case DW_OP_lit23:
    case DW_OP_lit24:
    case DW_OP_lit25:
    case DW_OP_lit26:
    case DW_OP_lit27:
    case DW_OP_lit28:
    case DW_OP_lit29:
    case DW_OP_lit30:
    case DW_OP_lit31:
        b.u = op - DW_OP_lit0;
        return push_expr_tkn(loc_expr, b);

    case DW_OP_reg0:
    case DW_OP_reg1:
    case DW_OP_reg2:
    case DW_OP_reg3:
    case DW_OP_reg4:
    case DW_OP_reg5:
    case DW_OP_reg6:
    case DW_OP_reg7:
    case DW_OP_reg8:
    case DW_OP_reg9:
    case DW_OP_reg10:
    case DW_OP_reg11:
    case DW_OP_reg12:
    case DW_OP_reg13:
    case DW_OP_reg14:
    case DW_OP_reg15:
    case DW_OP_reg16:
    case DW_OP_reg17:
    case DW_OP_reg18:
    case DW_OP_reg19:
    case DW_OP_reg20:
    case DW_OP_reg21:
    case DW_OP_reg22:
    case DW_OP_reg23:
    case DW_OP_reg24:
    case DW_OP_reg25:
    case DW_OP_reg26:
    case DW_OP_reg27:
    case DW_OP_reg28:
    case DW_OP_reg29:
    case DW_OP_reg30:
    case DW_OP_reg31:
        b.u = op - DW_OP_reg0;
        res = push_expr_tkn(loc_expr, b);
        if (res) return res;
        return RESULT_REGISTER;

    case DW_OP_breg0:
    case DW_OP_breg1:
    case DW_OP_breg2:
    case DW_OP_breg3:
    case DW_OP_breg4:
    case DW_OP_breg5:
    case DW_OP_breg6:
    case DW_OP_breg7:
    case DW_OP_breg8:
    case DW_OP_breg9:
    case DW_OP_breg10:
    case DW_OP_breg11:
    case DW_OP_breg12:
    case DW_OP_breg13:
    case DW_OP_breg14:
    case DW_OP_breg15:
    case DW_OP_breg16:
    case DW_OP_breg17:
    case DW_OP_breg18:
    case DW_OP_breg19:
    case DW_OP_breg20:
    case DW_OP_breg21:
    case DW_OP_breg22:
    case DW_OP_breg23:
    case DW_OP_breg24:
    case DW_OP_breg25:
    case DW_OP_breg26:
    case DW_OP_breg27:
    case DW_OP_breg28:
    case DW_OP_breg29:
    case DW_OP_breg30:
    case DW_OP_breg31:
        res = get_register(&loc_expr->loc_info.callee_regs_cur_value,
                           op - DW_OP_breg0, &p_reg);
        if (res) return res;
        b.u = *p_reg;
        return push_expr_tkn(loc_expr, b);

    case DW_OP_regx:
        b.u = op1;
        res = push_expr_tkn(loc_expr, b);
        if (res) return res;
        return RESULT_REGISTER;

    case DW_OP_fbreg:
        b.u = loc_expr->loc_info.frame_base + op1;
        return push_expr_tkn(loc_expr, b);

    case DW_OP_bregx:
        res = get_register(&loc_expr->loc_info.callee_regs_cur_value, op1,
                           &p_reg);
        if (res) return res;
        b.u += *p_reg + op2;
        return push_expr_tkn(loc_expr, b);

    case DW_OP_piece:
        *bytes = op1;
        return PARSE_PIECE;

    case DW_OP_nop:
        return 0;

    case DW_OP_push_object_address: /* DWARF3 */
        b.u = loc_expr->loc_info.obj_addr;
        return push_expr_tkn(loc_expr, b);

    case DW_OP_call_frame_cfa:              /* DWARF3f */
        b.u = loc_expr->loc_info.caller_sp; // Is correct?
        return push_expr_tkn(loc_expr, b);

    case DW_OP_implicit_value: /* DWARF4 */
        return RESULT_IMPLICIT_VALUE;
    case DW_OP_stack_value: /* DWARF4 */
        return RESULT_VALUE;

    case DW_OP_implicit_pointer: /* DWARF5 */
        // Implemented here but such a result is not useable by the debugger
        // currently.

        return RESULT_IMPLICIT_POINTER;

    case DW_OP_addrx:       /* DWARF5 */
    case DW_OP_constx:      /* DWARF5 */
    case DW_OP_entry_value: /* DWARF5 */
        // TODO: implement
        return UNIMPLEMENTED_OP_CODE;

    case DW_OP_bit_piece:        /* DWARF3f */
    case DW_OP_call2:            /* DWARF3 */
    case DW_OP_call4:            /* DWARF3 */
    case DW_OP_call_ref:         /* DWARF3 */
    case DW_OP_form_tls_address: /* DWARF3f */
    case DW_OP_const_type:       /* DWARF5 */
    case DW_OP_regval_type:      /* DWARF5 */
    case DW_OP_deref_type:       /* DWARF5 */
    case DW_OP_convert:          /* DWARF5 */
    case DW_OP_reinterpret:      /* DWARF5 */
        return UNIMPLEMENTED_OP_CODE;

    default:
        return INVALID_OPCODE;
    }
}

int parse_op_addr(Loc_Expr *loc_expr, UWORD op1) {
    return push_expr_tkn(
        loc_expr,
        (Value){.u = op1 + loc_expr->loc_info.load_address}); // is correct?
}

int parse_op_deref(Loc_Expr *loc_expr, UWORD size) {
    int res;

    Value b;
    res = pop_stack_top(loc_expr, &b);
    if (res) return res;

    res = __ptrace_peek_data(loc_expr->loc_info.c_pid, b.u, &b.u);
    if (res) return res;

    if (size > sizeof(&size)) {
        return 1; // This operand is a 1-byte unsigned integral constant whose
                  // value may not be larger than the size of the generic type.
    }

    if (size < sizeof(&size)) {
        b.u &= (1 << size) - 1;
    }

    res = push_expr_tkn(loc_expr, b);
    if (res) return res;

    return 0;
}

int parse_op_dup(Loc_Expr *loc_expr) {
    int res;

    Value b;
    res = pop_stack_top(loc_expr, &b);
    if (res) return res;

    if ((res = push_expr_tkn(loc_expr, b)) ||
        (res = push_expr_tkn(loc_expr, b))) {
        return res;
    }

    return 0;
}

int parse_op_pick(Loc_Expr *loc_expr, UWORD op1) {
    if (op1 >= loc_expr->stack_top) {
        return EXPR_INDEX_OOB;
    }

    return push_expr_tkn(loc_expr,
                         loc_expr->op_stack[loc_expr->stack_top - 1 - op1]);
}

int parse_op_swap(Loc_Expr *loc_expr) {
    int res;
    Value b, c;

    if ((res = pop_stack_top(loc_expr, &b)) ||
        (res = pop_stack_top(loc_expr, &c))) {
        return res;
    }

    if ((res = push_expr_tkn(loc_expr, b)) ||
        (res = push_expr_tkn(loc_expr, c))) {
        return res;
    }

    return 0;
}

int parse_op_rot(Loc_Expr *loc_expr) {
    int res;
    Value a, b, c;

    if ((res = pop_stack_top(loc_expr, &a)) ||
        (res = pop_stack_top(loc_expr, &b)) ||
        (res = pop_stack_top(loc_expr, &c))) {
        return res;
    }

    if ((res = push_expr_tkn(loc_expr, a)) ||
        (res = push_expr_tkn(loc_expr, c)) ||
        (res = push_expr_tkn(loc_expr, b))) {
        return res;
    }

    return 0;
}

int parse_op_abs(Loc_Expr *loc_expr) {
    int res;
    Value b;

    res = pop_stack_top(loc_expr, &b);
    if (res) return res;

    if (b.s < 0) {
        b.u = -b.s;
    }

    return push_expr_tkn(loc_expr, b);
}

int parse_op_plus_uconst(Loc_Expr *loc_expr, UWORD op1) {
    int res;
    Value b;

    res = pop_stack_top(loc_expr, &b);
    if (res) return res;

    b.u += op1;
    return push_expr_tkn(loc_expr, b);
}

int parse_op_plus_div(Loc_Expr *loc_expr) {
    int res;
    Value b, c;

    if ((res = pop_stack_top(loc_expr, &b)) ||
        (res = pop_stack_top(loc_expr, &c))) {
        return res;
    }

    b.u = c.s / b.s;
    return push_expr_tkn(loc_expr, b);
}

int parse_op_shl(Loc_Expr *loc_expr) {
    int res;
    Value b, c;

    if ((res = pop_stack_top(loc_expr, &b)) ||
        (res = pop_stack_top(loc_expr, &c))) {
        return res;
    }

    if (b.u >= sizeof(b.u) * 8) {
        b.u = 0;
        return push_expr_tkn(loc_expr, b);
    }

    b.u = (c.u >> b.u);
    return push_expr_tkn(loc_expr, b);
}

int parse_op_shr(Loc_Expr *loc_expr) {
    int res;
    Value b, c;

    if ((res = pop_stack_top(loc_expr, &b)) ||
        (res = pop_stack_top(loc_expr, &c))) {
        return res;
    }

    if (b.u >= sizeof(b.u) * 8) {
        b.u = 0;
        return push_expr_tkn(loc_expr, b);
    }

    b.u = (c.u << b.u);
    return push_expr_tkn(loc_expr, b);
}

int parse_op_shra(Loc_Expr *loc_expr) {
    int res, neg;
    Value b, c;

    if ((res = pop_stack_top(loc_expr, &b)) ||
        (res = pop_stack_top(loc_expr, &c))) {
        return res;
    }

    if (b.u >= sizeof(b.u) * 8) {
        b.u = 0;
        return push_expr_tkn(loc_expr, b);
    }

    neg = (c.s < 0);
    if (neg) c.s = -c.s;

    b.u = (c.u >> b.u);
    if (neg) b.s = -b.s;

    return push_expr_tkn(loc_expr, b);
}

int parse_op_bra(Loc_Expr *loc_expr, UWORD op1, int16_t *bytes) {
    int res;
    Value b;

    res = pop_stack_top(loc_expr, &b);
    if (res) return res;

    if (b.u != 0) {
        *bytes = op1;
    } else {
        *bytes = 0;
    }

    return 0;
}

int parse_op_div(Loc_Expr *loc_expr) {
    int res;
    Value b, c;
    if ((res = pop_stack_top(loc_expr, &b)) ||
        (res = pop_stack_top(loc_expr, &c))) {
        return res;
    }

    b.u = c.s / b.s;
    return push_expr_tkn(loc_expr, b);
}

// Copy from debugger.h - probably bad design
static int __ptrace_peek_data(UWORD c_pid, UWORD addr, UWORD *data) {
    errno = 0;
    *data = ptrace(PTRACE_PEEKDATA, c_pid, addr, NULL);
    if (*data == -1 && errno) {
        return 1;
    }
    return 0;
}