#include <stdio.h>
#include <string.h>

#include "mctypes.h"

#include "mccompile.h"
#include "mclops.h"
#include "mclstr.h"

#define NO_JUMP ((i64) -1)
#define INITIAL_CODE_CAP (16)
#define MULTRET (-1)
#define LFIELDS_PER_FLUSH (50)

#define TM_ADD  (6)
#define TM_SUB  (7)
#define TM_MUL  (8)
#define TM_MOD  (9)
#define TM_POW  (10)
#define TM_DIV  (11)
#define TM_IDIV (12)
#define TM_BAND (13)
#define TM_BOR  (14)
#define TM_BXOR (15)
#define TM_SHL  (16)
#define TM_SHR  (17)

typedef struct CmpDesc {
    u8 op;
    u8 swap;
    u8 invert_k;
} CmpDesc;


static i64 mccomp_emit(MCComp* cstate, u32 instr);
static void mccomp_patch(MCComp* cstate, i64 jmp_pc, u32 target);
static u8 mccomp_reserve(MCComp* cstate);
static i32 mccomp_resolve_local(MCComp* cstate, const char* name);
static i8 mccomp_local_is_const(MCComp* cstate, const char* name);
static i32 mccomp_resolve_upval(MCComp* cstate, const char* name);
static i8 mccomp_newlocal(MCComp* cstate, char* name, u8 reg);
static i32 mccomp_const_name(MCComp* cstate, char* name);
static i8 mccomp_cmp_describe(i64 tok, CmpDesc* out);
static i8 mccomp_arith_opcode(i64 tok, u8* out, u8* tm);
static i32 mccomp_emit_skip_placeholder(MCComp* cstate, const char* what);
static i32 mccomp_name(MCComp* cstate, heap_header* node);
static i32 mccomp_integer(MCComp* cstate, heap_header* node);
static i32 mccomp_float(MCComp* cstate, heap_header* node);
static i32 mccomp_compare_value(MCComp* cstate, heap_header* node);
static i32 mccomp_logical(MCComp* cstate, heap_header* node, u8 is_and);
static i32 mccomp_binop(MCComp* cstate, heap_header* node);
static i32 mccomp_unop(MCComp* cstate, heap_header* node);
static u8 mccomp_is_multi_exp(heap_header* node);
static i32 mccomp_vararg(MCComp* cstate, i32 want);
static i32 mccomp_call(MCComp* cstate, heap_header* node, i32 want);
static i32 mccomp_exp_multi(MCComp* cstate, heap_header* node, i32 want);
static u8 mccomp_int_key_fits(heap_header* key, u8* out);
static i32 mccomp_index(MCComp* cstate, heap_header* node);
static i8 mccomp_store(MCComp* cstate, heap_header* target, u8 valreg);
static i8 mccomp_table_set_idx(MCComp* cstate, u8 ra, heap_header* field,
        u8 base);
static i32 mccomp_table(MCComp* cstate, heap_header* node);
static i32 mccomp_exp(MCComp* cstate, heap_header* node);
static i64 mccomp_cond(MCComp* cstate, heap_header* node);
static const char* mccomp_attname_attr(heap_header* attname);
static i8 mccomp_local(MCComp* cstate, heap_header* stat);
static i8 mccomp_assign(MCComp* cstate, heap_header* stat);
static i8 mccomp_return(MCComp* cstate, heap_header* stat);
static i8 mccomp_if(MCComp* cstate, heap_header* stat);
static i8 mccomp_while(MCComp* cstate, heap_header* stat);
static i8 mccomp_repeat(MCComp* cstate, heap_header* stat);
static i8 mccomp_fornum(MCComp* cstate, heap_header* stat);
static i8 mccomp_forin(MCComp* cstate, heap_header* stat);
static i8 mccomp_break(MCComp* cstate);
static i8 mccomp_funcdef(MCComp* cstate, heap_header* stat);
static i32 mccomp_func_expr(MCComp* cstate, heap_header* node);
static i8 mccomp_localfunc(MCComp* cstate, heap_header* stat);
static i8 mccomp_label(MCComp* cstate, heap_header* stat);
static i8 mccomp_goto(MCComp* cstate, heap_header* stat);
static i8 mccomp_stat(MCComp* cstate, heap_header* stat);
static i8 mccomp_block_body(MCComp* cstate, heap_header* block);
static i8 mccomp_block(MCComp* cstate, heap_header* block);
static heap_header* mccomp_body(MCComp* cstate, char* name,
        heap_header* arglist, heap_header* block, u8 is_method);
static heap_header* mccomp_proto(MCComp* parent, char* name,
        heap_header* arglist, heap_header* block, u8 is_method);
static void mccomp_log_instr(u32 instr);
static void mccomp_log_consts(heap_header* func);

static i64 mccomp_emit(MCComp* cstate, u32 instr) {
    u32 pc = FUNCLEN(cstate);
    if (0 != mclfunc_append_instr(cstate->func, instr, cstate->heap)) {
        cstate->error = 1;
        return NO_JUMP;
    }

    if (cstate->freereg > cstate->maxreg) {
        cstate->maxreg = cstate->freereg;
    }
    return (i64) pc;
}

static void mccomp_patch(MCComp* cstate, i64 jmp_pc, u32 target) {
    u32* code = NULL;
    if (NO_JUMP == jmp_pc) {
        return;
    }

    code = mclfunc_getbody(cstate->func);
    code[jmp_pc] = CREATE_sJ(OP_JMP, (i32) target - ((i32) jmp_pc + 1));
}

static u8 mccomp_reserve(MCComp* cstate) {
    u8 reg = cstate->freereg;
    cstate->freereg += 1;
    if (cstate->freereg > cstate->maxreg) {
        cstate->maxreg = cstate->freereg;
    }
    return reg;
}

static i32 mccomp_resolve_local(MCComp* cstate, const char* name) {
    for (i32 idx = (i32) cstate->nactive - 1; idx >= 0; idx -= 1) {
        if (cstate->locals[idx].name == name) {
            return cstate->locals[idx].reg;
        }
    }
    return -1;
}

static i8 mccomp_local_is_const(MCComp* cstate, const char* name) {
    for (i32 idx = (i32) cstate->nactive - 1; idx >= 0; idx -= 1) {
        if (cstate->locals[idx].name == name) {
            return (1 == cstate->locals[idx].attrib);
        }
    }
    return 0;
}

static i32 mccomp_resolve_upval(MCComp* cstate, const char* name) {
    UpvalDesc* ups = mclfunc_getupvals(cstate->func);
    u8 n = cstate->func->object.function->num_upvals;
    i32 pidx = 0;
    i32 pup = 0;

    for (u8 idx = 0; idx < n; idx += 1) {
        if (ups[idx].name == name) {
            return idx;
        }
    }
    if (NULL == cstate->parent) {
        return -1;
    }

    pidx = mccomp_resolve_local(cstate->parent, name);
    if (pidx >= 0) {


        cstate->parent->has_capture = 1;
        return mclfunc_add_upval(cstate->func, (char*) name, 1, (u8) pidx,
                cstate->heap);
    }
    pup = mccomp_resolve_upval(cstate->parent, name);
    if (pup >= 0) {
        return mclfunc_add_upval(cstate->func, (char*) name, 0, (u8) pup,
                cstate->heap);
    }
    return -1;
}

static i8 mccomp_newlocal(MCComp* cstate, char* name, u8 reg) {
    if ((u32) cstate->nactive >= cstate->config->MAX_LOCALS) {
        printf("[CC] too many locals\n");
        cstate->error = 1;
        return -1;
    }
    cstate->locals[cstate->nactive].name = name;
    cstate->locals[cstate->nactive].reg = reg;
    cstate->locals[cstate->nactive].attrib = 0;
    cstate->nactive += 1;
    return 0;
}

static i32 mccomp_const_name(MCComp* cstate, char* name) {
    u32 len = (u32) strlen(name);
    heap_header* str = mclstr_alloc(len + 1, cstate->heap);

    if (NULL == str) {
        cstate->error = 1;
        return -1;
    }

    if (0 != mclstr_append_str0(str, name, len, cstate->heap)) {
        cstate->error = 1;
        return -1;
    }

    return mclfunc_const_str(cstate->func, str, cstate->heap);
}

static i8 mccomp_cmp_describe(i64 tok, CmpDesc* out) {
    switch (tok) {
        case '<':
            out->op = OP_LT;
            out->swap = 0;
            out->invert_k = 0;
            break;
        case '>':
            out->op = OP_LT;
            out->swap = 1;
            out->invert_k = 0;
            break;
        case TK_LE:
            out->op = OP_LE;
            out->swap = 0;
            out->invert_k = 0;
            break;
        case TK_GE:
            out->op = OP_LE;
            out->swap = 1;
            out->invert_k = 0;
            break;
        case TK_EQ:
            out->op = OP_EQ;
            out->swap = 0;
            out->invert_k = 0;
            break;
        case TK_NE:
            out->op = OP_EQ;
            out->swap = 0;
            out->invert_k = 1;
            break;
        default:
            return -1;
    }

    return 0;
}

static i8 mccomp_arith_opcode(i64 tok, u8* out, u8* tm) {
    switch (tok) {
        case '+':
            *out = OP_ADD;
            *tm = TM_ADD;
            break;
        case '-':
            *out = OP_SUB;
            *tm = TM_SUB;
            break;
        case '*':
            *out = OP_MUL;
            *tm = TM_MUL;
            break;
        case '/':
            *out = OP_DIV;
            *tm = TM_DIV;
            break;
        case '%':
            *out = OP_MOD;
            *tm = TM_MOD;
            break;
        case '^':
            *out = OP_POW;
            *tm = TM_POW;
            break;
        case TK_IDIV:
            *out = OP_IDIV;
            *tm = TM_IDIV;
            break;
        case '&':
            *out = OP_BAND;
            *tm = TM_BAND;
            break;
        case '|':
            *out = OP_BOR;
            *tm = TM_BOR;
            break;
        case '~':
            *out = OP_BXOR;
            *tm = TM_BXOR;
            break;
        case TK_SHL:
            *out = OP_SHL;
            *tm = TM_SHL;
            break;
        case TK_SHR:
            *out = OP_SHR;
            *tm = TM_SHR;
            break;
        default:
            return -1;
    }

    return 0;
}

static i32 mccomp_emit_skip_placeholder(MCComp* cstate, const char* what) {
    u8 reg = 0;
    printf("[CC] unsupported expression: %s\n", what);
    reg = mccomp_reserve(cstate);
    mccomp_emit(cstate, CREATE_ABCk(OP_LOADNIL, reg, 0, 0, 0));
    return reg;
}

static i32 mccomp_name(MCComp* cstate, heap_header* node) {
    char* name = AST_VAL(node).varname;
    i32 src = mccomp_resolve_local(cstate, name);
    i32 up = 0;
    i32 konst = 0;
    u8 dst = 0;

    if (src >= 0) {
        dst = mccomp_reserve(cstate);
        mccomp_emit(cstate, CREATE_ABCk(OP_MOVE, dst, (u8) src, 0, 0));
        return dst;
    }

    up = mccomp_resolve_upval(cstate, name);
    if (up >= 0) {
        dst = mccomp_reserve(cstate);
        mccomp_emit(cstate, CREATE_ABCk(OP_GETUPVAL, dst, (u8) up, 0, 0));
        return dst;
    }


    konst = mccomp_const_name(cstate, name);
    if (konst < 0) {
        return -1;
    }
    dst = mccomp_reserve(cstate);
    mccomp_emit(cstate, CREATE_ABCk(OP_GETTABUP, dst, 0, (u8) konst, 0));
    return dst;
}

static i32 mccomp_integer(MCComp* cstate, heap_header* node) {
    i64 v = AST_VAL(node).integer;
    i32 konst = 0;
    u8 reg = mccomp_reserve(cstate);

    if ((v < -OFFSET_sBx) || (v > OFFSET_sBx)) {
        konst = mclfunc_const_int(cstate->func, v, cstate->heap);
        if (konst < 0) {
            cstate->error = 1;
            return -1;
        }
        mccomp_emit(cstate, CREATE_ABx(OP_LOADK, reg, (u32) konst));
        return reg;
    }
    mccomp_emit(cstate, CREATE_AsBx(OP_LOADI, reg, (i32) v));
    return reg;
}

static i32 mccomp_float(MCComp* cstate, heap_header* node) {
    f64 v = AST_VAL(node).number;
    i64 iv = (i64) v;
    i32 konst = 0;
    u8 reg = mccomp_reserve(cstate);

    if (((f64) iv == v) && (iv >= -OFFSET_sBx) && (iv <= OFFSET_sBx)) {
        mccomp_emit(cstate, CREATE_AsBx(OP_LOADF, reg, (i32) iv));
        return reg;
    }

    konst = mclfunc_const_flt(cstate->func, v, cstate->heap);
    if (konst < 0) {
        cstate->error = 1;
        return -1;
    }
    mccomp_emit(cstate, CREATE_ABx(OP_LOADK, reg, (u32) konst));
    return reg;
}

static i32 mccomp_compare_value(MCComp* cstate, heap_header* node) {
    CmpDesc desc = {0, 0, 0};
    i32 ra = 0;
    i32 rb = 0;
    u8 lhs = 0;
    u8 rhs = 0;
    u8 k = 0;

    if (0 != mccomp_cmp_describe(AST_VAL(node).integer, &desc)) {
        return mccomp_emit_skip_placeholder(cstate, "comparison operator");
    }

    ra = mccomp_exp(cstate, AST_CHILD(node, 0));
    rb = mccomp_exp(cstate, AST_CHILD(node, 1));
    if ((ra < 0) || (rb < 0)) {
        return -1;
    }

    lhs = desc.swap ? (u8) rb : (u8) ra;
    rhs = desc.swap ? (u8) ra : (u8) rb;
    k = desc.invert_k ? 0 : 1;

    mccomp_emit(cstate, CREATE_ABCk(desc.op, lhs, rhs, 0, k));
    mccomp_emit(cstate, CREATE_sJ(OP_JMP, 1));
    mccomp_emit(cstate, CREATE_ABCk(OP_LFALSESKIP, (u8) ra, 0, 0, 0));
    mccomp_emit(cstate, CREATE_ABCk(OP_LOADTRUE, (u8) ra, 0, 0, 0));

    cstate->freereg = (u8) ra + 1;
    return ra;
}




static i32 mccomp_logical(MCComp* cstate, heap_header* node, u8 is_and) {
    u8 dst = cstate->freereg;
    i32 la = 0;
    i64 jmp = NO_JUMP;

    la = mccomp_exp(cstate, AST_CHILD(node, 0));
    if (la < 0) {
        return -1;
    }

    mccomp_emit(cstate, CREATE_ABCk(OP_TESTSET, dst, (u8) la, 0, is_and ? 0 : 1));
    jmp = mccomp_emit(cstate, CREATE_sJ(OP_JMP, 0));

    cstate->freereg = dst;
    if (mccomp_exp(cstate, AST_CHILD(node, 1)) < 0) {
        return -1;
    }

    mccomp_patch(cstate, jmp, FUNCLEN(cstate));
    cstate->freereg = (u8) (dst + 1);
    return dst;
}

static i32 mccomp_binop(MCComp* cstate, heap_header* node) {
    i64 tok = AST_VAL(node).integer;
    u8 op = 0;
    u8 tm = 0;
    i32 ra = 0;
    i32 rb = 0;
    CmpDesc desc = {0, 0, 0};

    if (TK_AND == tok) {
        return mccomp_logical(cstate, node, 1);
    }
    if (TK_OR == tok) {
        return mccomp_logical(cstate, node, 0);
    }
    if (0 == mccomp_cmp_describe(tok, &desc)) {
        return mccomp_compare_value(cstate, node);
    }

    ra = mccomp_exp(cstate, AST_CHILD(node, 0));
    rb = mccomp_exp(cstate, AST_CHILD(node, 1));
    if ((ra < 0) || (rb < 0)) {
        return -1;
    }

    if (TK_CONCAT == tok) {
        mccomp_emit(cstate, CREATE_ABCk(OP_CONCAT, (u8) ra, 2, 0, 0));
    } else if (0 == mccomp_arith_opcode(tok, &op, &tm)) {
        mccomp_emit(cstate, CREATE_ABCk(op, (u8) ra, (u8) ra, (u8) rb, 0));
        mccomp_emit(cstate, CREATE_ABCk(OP_MMBIN, (u8) ra, (u8) rb, tm, 0));
    } else {
        return mccomp_emit_skip_placeholder(cstate, "binary operator");
    }

    cstate->freereg = (u8) ra + 1;
    return ra;
}

static i32 mccomp_unop(MCComp* cstate, heap_header* node) {
    i64 tok = AST_VAL(node).integer;
    i32 rc = 0;
    u8 op = 0;

    rc = mccomp_exp(cstate, AST_CHILD(node, 0));
    if (rc < 0) {
        return -1;
    }

    switch (tok) {
        case '-':    op = OP_UNM;  break;
        case '~':    op = OP_BNOT; break;
        case '#':    op = OP_LEN;  break;
        case TK_NOT: op = OP_NOT;  break;
        default:
            return mccomp_emit_skip_placeholder(cstate, "unary operator");
    }

    mccomp_emit(cstate, CREATE_ABCk(op, (u8) rc, (u8) rc, 0, 0));
    cstate->freereg = (u8) rc + 1;
    return rc;
}

static u8 mccomp_is_multi_exp(heap_header* node) {
    ParseNodeType t = AST_TYPE(node);
    return (PN_CALL == t) || (PN_CALL_METHOD == t) || (PN_VARARG == t);
}

static i32 mccomp_vararg(MCComp* cstate, i32 want) {
    u8 reg = mccomp_reserve(cstate);
    mccomp_emit(cstate, CREATE_ABCk(OP_VARARG, reg, 0,
            (u8) ((MULTRET == want) ? 0 : (want + 1)), 0));
    cstate->freereg = (u8) ((MULTRET == want) ? reg : reg + want);
    return reg;
}


static i32 mccomp_call(MCComp* cstate, heap_header* node, i32 want) {
    u8 base = cstate->freereg;
    u8 is_method = (PN_CALL_METHOD == AST_TYPE(node));
    u32 nchild = AST_NCHILD(node);
    u32 nargs = 0;
    u32 idx = 0;
    u32 firstarg = 1;
    u8 open_args = 0;
    u8 b = 0;
    u8 c = 0;
    i32 konst = 0;
    heap_header* arg = NULL;

    if (mccomp_exp(cstate, AST_CHILD(node, 0)) < 0) {
        return -1;
    }

    if (is_method) {
        konst = mccomp_const_name(cstate, AST_VAL(node).varname);
        if (konst < 0) {
            return -1;
        }
        mccomp_emit(cstate, CREATE_ABCk(OP_SELF, base, base, (u8) konst, 1));
        cstate->freereg = base + 2;
        nargs += 1;
    }

    for (idx = firstarg; idx < nchild; idx += 1) {
        arg = AST_CHILD(node, idx);
        if ((idx + 1 == nchild) && mccomp_is_multi_exp(arg)) {
            if (mccomp_exp_multi(cstate, arg, MULTRET) < 0) {
                return -1;
            }
            open_args = 1;
        } else {
            if (mccomp_exp(cstate, arg) < 0) {
                return -1;
            }
        }
        nargs += 1;
    }

    b = (u8) (open_args ? 0 : (nargs + 1));
    c = (u8) ((MULTRET == want) ? 0 : (want + 1));
    mccomp_emit(cstate, CREATE_ABCk(OP_CALL, base, b, c, 0));
    cstate->freereg = (u8) ((MULTRET == want) ? base : base + want);
    return base;
}

static i32 mccomp_exp_multi(MCComp* cstate, heap_header* node, i32 want) {
    switch (AST_TYPE(node)) {
        case PN_CALL:
        case PN_CALL_METHOD:
            return mccomp_call(cstate, node, want);
        case PN_VARARG:
            return mccomp_vararg(cstate, want);
        default:
            return mccomp_exp(cstate, node);
    }
}


static u8 mccomp_int_key_fits(heap_header* key, u8* out) {
    if ((PN_INTEGER == AST_TYPE(key))
            && (AST_VAL(key).integer >= 0)
            && (AST_VAL(key).integer <= 255)) {
        *out = (u8) AST_VAL(key).integer;
        return 1;
    }
    return 0;
}


static i32 mccomp_index(MCComp* cstate, heap_header* node) {
    i32 tbl = mccomp_exp(cstate, AST_CHILD(node, 0));
    i32 konst = 0;
    i32 kr = 0;
    u8 ik = 0;
    heap_header* key = NULL;

    if (tbl < 0) {
        return -1;
    }

    if (PN_FIELD == AST_TYPE(node)) {
        konst = mccomp_const_name(cstate, AST_VAL(node).varname);
        if (konst < 0) {
            return -1;
        }
        mccomp_emit(cstate, CREATE_ABCk(OP_GETFIELD, (u8) tbl, (u8) tbl, (u8) konst, 0));
        cstate->freereg = (u8) (tbl + 1);
        return tbl;
    }

    key = AST_CHILD(node, 1);
    if (mccomp_int_key_fits(key, &ik)) {
        mccomp_emit(cstate, CREATE_ABCk(OP_GETI, (u8) tbl, (u8) tbl, ik, 0));
        cstate->freereg = (u8) (tbl + 1);
        return tbl;
    }
    if (PN_STRING == AST_TYPE(key)) {
        konst = mclfunc_const_str(cstate->func, AST_VAL(key).heapobj, cstate->heap);
        if (konst < 0) {
            cstate->error = 1;
            return -1;
        }
        mccomp_emit(cstate, CREATE_ABCk(OP_GETFIELD, (u8) tbl, (u8) tbl, (u8) konst, 0));
        cstate->freereg = (u8) (tbl + 1);
        return tbl;
    }
    kr = mccomp_exp(cstate, key);
    if (kr < 0) {
        return -1;
    }
    mccomp_emit(cstate, CREATE_ABCk(OP_GETTABLE, (u8) tbl, (u8) tbl, (u8) kr, 0));
    cstate->freereg = (u8) (tbl + 1);
    return tbl;
}



static i8 mccomp_store(MCComp* cstate, heap_header* target, u8 valreg) {
    i32 tbl = 0;
    i32 konst = 0;
    i32 kr = 0;
    u8 ik = 0;
    heap_header* key = NULL;

    tbl = mccomp_exp(cstate, AST_CHILD(target, 0));
    if (tbl < 0) {
        return -1;
    }

    if (PN_FIELD == AST_TYPE(target)) {
        konst = mccomp_const_name(cstate, AST_VAL(target).varname);
        if (konst < 0) {
            return -1;
        }
        mccomp_emit(cstate, CREATE_ABCk(OP_SETFIELD, (u8) tbl, (u8) konst, valreg, 0));
        return 0;
    }

    key = AST_CHILD(target, 1);
    if (mccomp_int_key_fits(key, &ik)) {
        mccomp_emit(cstate, CREATE_ABCk(OP_SETI, (u8) tbl, ik, valreg, 0));
        return 0;
    }
    if (PN_STRING == AST_TYPE(key)) {
        konst = mclfunc_const_str(cstate->func, AST_VAL(key).heapobj, cstate->heap);
        if (konst < 0) {
            cstate->error = 1;
            return -1;
        }
        mccomp_emit(cstate, CREATE_ABCk(OP_SETFIELD, (u8) tbl, (u8) konst, valreg, 0));
        return 0;
    }
    kr = mccomp_exp(cstate, key);
    if (kr < 0) {
        return -1;
    }
    mccomp_emit(cstate, CREATE_ABCk(OP_SETTABLE, (u8) tbl, (u8) kr, valreg, 0));
    return 0;
}

static i8 mccomp_table_set_idx(MCComp* cstate, u8 ra, heap_header* field, u8 base) {
    heap_header* key = AST_CHILD(field, 0);
    heap_header* val = AST_CHILD(field, 1);
    i32 konst = 0;
    i32 kr = 0;
    i32 vr = 0;
    u8 ik = 0;

    cstate->freereg = base;
    if (mccomp_int_key_fits(key, &ik)) {
        vr = mccomp_exp(cstate, val);
        if (vr < 0) {
            return -1;
        }
        mccomp_emit(cstate, CREATE_ABCk(OP_SETI, ra, ik, (u8) vr, 0));
        return 0;
    }
    if (PN_STRING == AST_TYPE(key)) {
        konst = mclfunc_const_str(cstate->func, AST_VAL(key).heapobj, cstate->heap);
        if (konst < 0) {
            cstate->error = 1;
            return -1;
        }
        vr = mccomp_exp(cstate, val);
        if (vr < 0) {
            return -1;
        }
        mccomp_emit(cstate, CREATE_ABCk(OP_SETFIELD, ra, (u8) konst, (u8) vr, 0));
        return 0;
    }
    kr = mccomp_exp(cstate, key);
    if (kr < 0) {
        return -1;
    }
    vr = mccomp_exp(cstate, val);
    if (vr < 0) {
        return -1;
    }
    mccomp_emit(cstate, CREATE_ABCk(OP_SETTABLE, ra, (u8) kr, (u8) vr, 0));
    return 0;
}

static i32 mccomp_table(MCComp* cstate, heap_header* node) {
    u8 ra = mccomp_reserve(cstate);
    i64 newpc = 0;
    u32 nchild = AST_NCHILD(node);
    u32 idx = 0;
    u32 pending = 0;
    u32 arrtotal = 0;
    u32 hashcount = 0;
    u8 last_open = 0;
    heap_header* field = NULL;
    heap_header* val = NULL;
    i32 konst = 0;
    i32 vr = 0;
    u8 save = 0;
    u8 is_last = 0;

    newpc = mccomp_emit(cstate, CREATE_ABCk(OP_NEWTABLE, ra, 0, 0, 0));
    mccomp_emit(cstate, CREATE_Ax(OP_EXTRAARG, 0));

    for (idx = 0; idx < nchild; idx += 1) {
        field = AST_CHILD(node, idx);
        switch (AST_TYPE(field)) {
            case PN_FIELD_VAL:
                val = AST_CHILD(field, 0);
                is_last = (idx + 1 == nchild);
                cstate->freereg = (u8) (ra + 1 + pending);
                if (is_last && mccomp_is_multi_exp(val)) {
                    if (mccomp_exp_multi(cstate, val, MULTRET) < 0) {
                        return -1;
                    }
                    last_open = 1;
                } else if (mccomp_exp(cstate, val) < 0) {
                    return -1;
                }
                pending += 1;
                arrtotal += 1;
                if (pending >= LFIELDS_PER_FLUSH) {
                    mccomp_emit(cstate, CREATE_ABCk(OP_SETLIST, ra,
                            (u8) pending, 0, 0));
                    pending = 0;
                    cstate->freereg = (u8) (ra + 1);
                }
                break;
            case PN_FIELD_NAME:
                save = (u8) (ra + 1 + pending);
                konst = mccomp_const_name(cstate, AST_VAL(field).varname);
                if (konst < 0) {
                    return -1;
                }
                cstate->freereg = save;
                vr = mccomp_exp(cstate, AST_CHILD(field, 0));
                if (vr < 0) {
                    return -1;
                }
                mccomp_emit(cstate, CREATE_ABCk(OP_SETFIELD, ra, (u8) konst,
                        (u8) vr, 0));
                cstate->freereg = save;
                hashcount += 1;
                break;
            case PN_FIELD_IDX:
                save = (u8) (ra + 1 + pending);
                if (0 != mccomp_table_set_idx(cstate, ra, field, save)) {
                    return -1;
                }
                cstate->freereg = save;
                hashcount += 1;
                break;
            default:
                break;
        }
    }

    if ((pending > 0) || last_open) {
        mccomp_emit(cstate, CREATE_ABCk(OP_SETLIST, ra,
                (u8) (last_open ? 0 : pending), 0, 0));
    }

    mclfunc_getbody(cstate->func)[newpc] =
            CREATE_ABCk(OP_NEWTABLE, ra, (u8) hashcount, (u8) arrtotal, 0);
    cstate->freereg = (u8) (ra + 1);
    return ra;
}

static i32 mccomp_exp(MCComp* cstate, heap_header* node) {
    i32 konst = 0;
    u8 reg = 0;

    if (NULL == node) {
        return -1;
    }

    switch (AST_TYPE(node)) {
        case PN_INTEGER:
            return mccomp_integer(cstate, node);
        case PN_FLOAT:
            return mccomp_float(cstate, node);
        case PN_NIL:
            reg = mccomp_reserve(cstate);
            mccomp_emit(cstate, CREATE_ABCk(OP_LOADNIL, reg, 0, 0, 0));
            return reg;
        case PN_TRUE:
            reg = mccomp_reserve(cstate);
            mccomp_emit(cstate, CREATE_ABCk(OP_LOADTRUE, reg, 0, 0, 0));
            return reg;
        case PN_FALSE:
            reg = mccomp_reserve(cstate);
            mccomp_emit(cstate, CREATE_ABCk(OP_LOADFALSE, reg, 0, 0, 0));
            return reg;
        case PN_NAME:
            return mccomp_name(cstate, node);
        case PN_BINOP:
            return mccomp_binop(cstate, node);
        case PN_UNOP:
            return mccomp_unop(cstate, node);
        case PN_STRING:
            konst = mclfunc_const_str(cstate->func, AST_VAL(node).heapobj, cstate->heap);
            if (konst < 0) {
                cstate->error = 1;
                return -1;
            }
            reg = mccomp_reserve(cstate);
            mccomp_emit(cstate, CREATE_ABx(OP_LOADK, reg, (u32) konst));
            return reg;
        case PN_CALL:
        case PN_CALL_METHOD:
            return mccomp_call(cstate, node, 1);
        case PN_VARARG:
            return mccomp_vararg(cstate, 1);
        case PN_FUNC_EXPR:
            return mccomp_func_expr(cstate, node);
        case PN_PAREN:

            return mccomp_exp(cstate, AST_CHILD(node, 0));
        case PN_TABLE:
            return mccomp_table(cstate, node);
        case PN_INDEX:
        case PN_FIELD:
            return mccomp_index(cstate, node);
        default:
            return mccomp_emit_skip_placeholder(cstate, "expression");
    }
}

static i64 mccomp_cond(MCComp* cstate, heap_header* node) {
    CmpDesc desc = {0, 0, 0};
    i32 ra = 0;
    i32 rb = 0;
    i32 reg = 0;
    u8 lhs = 0;
    u8 rhs = 0;

    if ((PN_BINOP == AST_TYPE(node))
            && (0 == mccomp_cmp_describe(AST_VAL(node).integer, &desc))) {
        ra = mccomp_exp(cstate, AST_CHILD(node, 0));
        rb = mccomp_exp(cstate, AST_CHILD(node, 1));
        if ((ra < 0) || (rb < 0)) {
            return NO_JUMP;
        }
        lhs = desc.swap ? (u8) rb : (u8) ra;
        rhs = desc.swap ? (u8) ra : (u8) rb;
        mccomp_emit(cstate, CREATE_ABCk(desc.op, lhs, rhs, 0, desc.invert_k ? 1 : 0));
        cstate->freereg = cstate->nactive;
        return mccomp_emit(cstate, CREATE_sJ(OP_JMP, 0));
    }

    reg = mccomp_exp(cstate, node);
    if (reg < 0) {
        return NO_JUMP;
    }
    mccomp_emit(cstate, CREATE_ABCk(OP_TEST, (u8) reg, 0, 0, 0));
    cstate->freereg = cstate->nactive;
    return mccomp_emit(cstate, CREATE_sJ(OP_JMP, 0));
}


static const char* mccomp_attname_attr(heap_header* attname) {
    if (AST_NCHILD(attname) > 0) {
        return AST_VAL(AST_CHILD(attname, 0)).varname;
    }
    return NULL;
}

static i8 mccomp_local(MCComp* cstate, heap_header* stat) {
    u32 nname = (u32) AST_VAL(stat).integer;
    u32 nchild = AST_NCHILD(stat);
    u32 nexpr = nchild - nname;
    u8 base = cstate->nactive;
    u32 idx = 0;
    u32 produced = 0;
    u8 has_close = 0;
    const char* attr = NULL;
    heap_header* e = NULL;
    i32 want = 0;

    for (idx = 0; idx < nexpr; idx += 1) {
        e = AST_CHILD(stat, nname + idx);
        cstate->freereg = (u8) (base + produced);
        if ((idx + 1 == nexpr) && mccomp_is_multi_exp(e)) {
            want = (nname > produced) ? (i32) (nname - produced) : 1;
            if (mccomp_exp_multi(cstate, e, want) < 0) {
                return -1;
            }
            produced += (u32) want;
        } else {
            if (mccomp_exp(cstate, e) < 0) {
                return -1;
            }
            produced += 1;
        }
    }

    if (produced < nname) {
        mccomp_emit(cstate, CREATE_ABCk(OP_LOADNIL,
                (u8) (base + produced), (u8) (nname - produced - 1), 0, 0));
    }

    cstate->freereg = (u8) (base + nname);
    for (idx = 0; idx < nname; idx += 1) {
        if (0 != mccomp_newlocal(cstate,
                AST_VAL(AST_CHILD(stat, idx)).varname, (u8) (base + idx))) {
            return -1;
        }
        attr = mccomp_attname_attr(AST_CHILD(stat, idx));
        if (NULL != attr) {
            if (0 == strcmp(attr, "const")) {
                cstate->locals[cstate->nactive - 1].attrib = 1;
            } else if (0 == strcmp(attr, "close")) {
                cstate->locals[cstate->nactive - 1].attrib = 2;
                mccomp_emit(cstate, CREATE_ABCk(OP_TBC, (u8) (base + idx), 0, 0, 0));
                has_close = 1;
            }
        }
    }

    if (has_close) {
        cstate->has_tbc = 1;
    }
    return 0;
}

static i8 mccomp_assign(MCComp* cstate, heap_header* stat) {
    u32 ntarget = (u32) AST_VAL(stat).integer;
    u32 nchild = AST_NCHILD(stat);
    u32 nval = nchild - ntarget;
    u8 base = cstate->freereg;
    u32 idx = 0;
    u32 produced = 0;
    u8 scratch = 0;
    heap_header* target = NULL;
    heap_header* e = NULL;
    i32 dst = 0;
    i32 up = 0;
    i32 konst = 0;
    i32 want = 0;


    for (idx = 0; idx < nval; idx += 1) {
        e = AST_CHILD(stat, ntarget + idx);
        cstate->freereg = (u8) (base + produced);
        if ((idx + 1 == nval) && mccomp_is_multi_exp(e)) {
            want = (ntarget > produced) ? (i32) (ntarget - produced) : 1;
            if (mccomp_exp_multi(cstate, e, want) < 0) {
                return -1;
            }
            produced += (u32) want;
        } else {
            if (mccomp_exp(cstate, e) < 0) {
                return -1;
            }
            produced += 1;
        }
    }
    if (produced < ntarget) {
        mccomp_emit(cstate, CREATE_ABCk(OP_LOADNIL,
                (u8) (base + produced), (u8) (ntarget - produced - 1), 0, 0));
    }
    scratch = (u8) (base + ntarget);
    cstate->freereg = scratch;

    for (idx = 0; idx < ntarget; idx += 1) {
        target = AST_CHILD(stat, idx);
        if (PN_NAME == AST_TYPE(target)) {
            dst = mccomp_resolve_local(cstate, AST_VAL(target).varname);
            if (dst >= 0) {
                if (0 != mccomp_local_is_const(cstate, AST_VAL(target).varname)) {
                    printf("[CC] cannot assign to const '%s'\n",
                            AST_VAL(target).varname);
                    cstate->error = 1;
                    return -1;
                }
                mccomp_emit(cstate, CREATE_ABCk(OP_MOVE,
                        (u8) dst, (u8) (base + idx), 0, 0));
                continue;
            }
            up = mccomp_resolve_upval(cstate, AST_VAL(target).varname);
            if (up >= 0) {
                mccomp_emit(cstate, CREATE_ABCk(OP_SETUPVAL,
                        (u8) (base + idx), (u8) up, 0, 0));
                continue;
            }
            konst = mccomp_const_name(cstate, AST_VAL(target).varname);
            if (konst < 0) {
                return -1;
            }
            mccomp_emit(cstate, CREATE_ABCk(OP_SETTABUP,
                    0, (u8) konst, (u8) (base + idx), 0));
            continue;
        }

        cstate->freereg = scratch;
        if (0 != mccomp_store(cstate, target, (u8) (base + idx))) {
            return -1;
        }
    }

    cstate->freereg = cstate->nactive;
    return 0;
}

static i8 mccomp_return(MCComp* cstate, heap_header* stat) {
    u32 nret = AST_NCHILD(stat);
    u8 base = cstate->nactive;
    u32 idx = 0;
    u8 open = 0;
    heap_header* e = NULL;

    cstate->freereg = base;
    if (0 == nret) {
        mccomp_emit(cstate, CREATE_ABCk(OP_RETURN0, base, 0, 0, 0));
        return 0;
    }

    for (idx = 0; idx < nret; idx += 1) {
        e = AST_CHILD(stat, idx);
        if ((idx + 1 == nret) && mccomp_is_multi_exp(e)) {
            if (mccomp_exp_multi(cstate, e, MULTRET) < 0) {
                return -1;
            }
            open = 1;
        } else if (mccomp_exp(cstate, e) < 0) {
            return -1;
        }
    }




    mccomp_emit(cstate, CREATE_ABCk(OP_RETURN, base,
            (u8) (open ? 0 : (nret + 1)),
            (u8) (cstate->func->object.function->is_vararg
                    ? (cstate->func->object.function->num_params + 1) : 0),
            (cstate->has_tbc || cstate->has_capture) ? 1 : 0));
    cstate->freereg = base;
    return 0;
}

static i8 mccomp_if(MCComp* cstate, heap_header* stat) {
    u32 nchild = AST_NCHILD(stat);
    u32 idx = 0;
    i64 jfalse = NO_JUMP;
    i64 endjumps[64];
    u8 nend = 0;
    heap_header* clause = NULL;

    jfalse = mccomp_cond(cstate, AST_CHILD(stat, 0));
    if (0 != mccomp_block(cstate, AST_CHILD(stat, 1))) {
        return -1;
    }

    for (idx = 2; idx < nchild; idx += 1) {
        clause = AST_CHILD(stat, idx);

        if (nend < 64) {
            endjumps[nend] = mccomp_emit(cstate, CREATE_sJ(OP_JMP, 0));
            nend += 1;
        }
        mccomp_patch(cstate, jfalse, FUNCLEN(cstate));
        jfalse = NO_JUMP;

        if (PN_ELSEIF == AST_TYPE(clause)) {
            jfalse = mccomp_cond(cstate, AST_CHILD(clause, 0));
            if (0 != mccomp_block(cstate, AST_CHILD(clause, 1))) {
                return -1;
            }
        } else {
            if (0 != mccomp_block(cstate, AST_CHILD(clause, 0))) {
                return -1;
            }
        }
    }

    mccomp_patch(cstate, jfalse, FUNCLEN(cstate));
    for (idx = 0; idx < nend; idx += 1) {
        mccomp_patch(cstate, endjumps[idx], FUNCLEN(cstate));
    }
    return 0;
}

static i8 mccomp_while(MCComp* cstate, heap_header* stat) {
    u32 top = FUNCLEN(cstate);
    i64 jfalse = NO_JUMP;
    u8 saved_breaks = cstate->nbreaks;
    u32 idx = 0;

    cstate->in_loop += 1;
    jfalse = mccomp_cond(cstate, AST_CHILD(stat, 0));
    if (0 != mccomp_block(cstate, AST_CHILD(stat, 1))) {
        return -1;
    }
    mccomp_emit(cstate, CREATE_sJ(OP_JMP,
            (i32) top - ((i32) FUNCLEN(cstate) + 1)));

    mccomp_patch(cstate, jfalse, FUNCLEN(cstate));
    for (idx = saved_breaks; idx < cstate->nbreaks; idx += 1) {
        mccomp_patch(cstate, cstate->breaks[idx], FUNCLEN(cstate));
    }
    cstate->nbreaks = saved_breaks;
    cstate->in_loop -= 1;
    return 0;
}

static i8 mccomp_repeat(MCComp* cstate, heap_header* stat) {
    u32 top = FUNCLEN(cstate);
    u8 saved = cstate->nactive;
    u8 saved_breaks = cstate->nbreaks;
    i64 jback = NO_JUMP;
    u32 idx = 0;

    cstate->in_loop += 1;
    if (0 != mccomp_block_body(cstate, AST_CHILD(stat, 0))) {
        return -1;
    }
    jback = mccomp_cond(cstate, AST_CHILD(stat, 1));
    mccomp_patch(cstate, jback, top);

    for (idx = saved_breaks; idx < cstate->nbreaks; idx += 1) {
        mccomp_patch(cstate, cstate->breaks[idx], FUNCLEN(cstate));
    }
    cstate->nbreaks = saved_breaks;
    cstate->in_loop -= 1;
    cstate->nactive = saved;
    cstate->freereg = saved;
    return 0;
}

static i8 mccomp_fornum(MCComp* cstate, heap_header* stat) {
    u32 nchild = AST_NCHILD(stat);
    u8 has_step = (4 == nchild);
    u8 base = cstate->nactive;
    heap_header* block = AST_CHILD(stat, nchild - 1);
    i64 prep_pc = NO_JUMP;
    i64 loop_pc = NO_JUMP;
    u32 bodystart = 0;

    if (mccomp_exp(cstate, AST_CHILD(stat, 0)) < 0) {
        return -1;
    }
    if (mccomp_exp(cstate, AST_CHILD(stat, 1)) < 0) {
        return -1;
    }
    if (has_step) {
        if (mccomp_exp(cstate, AST_CHILD(stat, 2)) < 0) {
            return -1;
        }
    } else {
        mccomp_emit(cstate, CREATE_AsBx(OP_LOADI, (u8) (base + 2), 1));
        cstate->freereg += 1;
    }


    cstate->freereg = (u8) (base + 3);
    if (0 != mccomp_newlocal(cstate, AST_VAL(stat).varname, (u8) (base + 3))) {
        return -1;
    }
    cstate->freereg = (u8) (base + 4);
    if (cstate->freereg > cstate->maxreg) {
        cstate->maxreg = cstate->freereg;
    }

    prep_pc = mccomp_emit(cstate, CREATE_ABx(OP_FORPREP, base, 0));
    bodystart = FUNCLEN(cstate);
    if (0 != mccomp_block(cstate, block)) {
        return -1;
    }
    loop_pc = FUNCLEN(cstate);
    mccomp_emit(cstate, CREATE_ABx(OP_FORLOOP, base, (u32) loop_pc - bodystart + 1));

    if (NO_JUMP != prep_pc) {
        mclfunc_getbody(cstate->func)[prep_pc] =
                CREATE_ABx(OP_FORPREP, base, (u32) loop_pc - bodystart);
    }

    cstate->nactive = base;
    cstate->freereg = base;
    return 0;
}





static i8 mccomp_forin(MCComp* cstate, heap_header* stat) {
    u32 nchild = AST_NCHILD(stat);
    u32 nnames = (u32) AST_VAL(stat).integer;
    u32 nexp = nchild - nnames - 1;
    u8 base = cstate->nactive;
    heap_header* block = AST_CHILD(stat, nchild - 1);
    heap_header* e = NULL;
    i64 prep_pc = NO_JUMP;
    i64 call_pc = NO_JUMP;
    i64 loop_pc = NO_JUMP;
    u32 bodystart = 0;
    u32 produced = 0;
    u32 idx = 0;
    u8 saved_breaks = cstate->nbreaks;
    i32 want = 0;


    cstate->freereg = base;
    for (idx = 0; idx < nexp; idx += 1) {
        e = AST_CHILD(stat, nnames + idx);
        cstate->freereg = (u8) (base + produced);
        if ((idx + 1 == nexp) && mccomp_is_multi_exp(e)) {
            want = (4 > (i32) produced) ? (i32) (4 - produced) : 1;
            if (mccomp_exp_multi(cstate, e, want) < 0) {
                return -1;
            }
            produced += (u32) want;
        } else {
            if (mccomp_exp(cstate, e) < 0) {
                return -1;
            }
            produced += 1;
        }
    }
    if (produced < 4) {
        mccomp_emit(cstate, CREATE_ABCk(OP_LOADNIL,
                (u8) (base + produced), (u8) (4 - produced - 1), 0, 0));
    }


    {
        char* statename = mcstrtbl_intern(cstate->strtbl, "(for state)", 11);
        for (idx = 0; idx < 4; idx += 1) {
            if (0 != mccomp_newlocal(cstate, statename, (u8) (base + idx))) {
                return -1;
            }
        }
    }
    cstate->freereg = (u8) (base + 4);

    prep_pc = mccomp_emit(cstate, CREATE_ABx(OP_TFORPREP, base, 0));

    for (idx = 0; idx < nnames; idx += 1) {
        if (0 != mccomp_newlocal(cstate, AST_VAL(AST_CHILD(stat, idx)).varname,
                (u8) (base + 4 + idx))) {
            return -1;
        }
    }
    cstate->freereg = (u8) (base + 4 + nnames);
    if (cstate->freereg > cstate->maxreg) {
        cstate->maxreg = cstate->freereg;
    }

    bodystart = FUNCLEN(cstate);
    cstate->in_loop += 1;
    if (0 != mccomp_block(cstate, block)) {
        return -1;
    }
    cstate->in_loop -= 1;

    call_pc = FUNCLEN(cstate);
    mccomp_emit(cstate, CREATE_ABCk(OP_TFORCALL, base, 0, (u8) nnames, 0));
    loop_pc = mccomp_emit(cstate, CREATE_ABx(OP_TFORLOOP, base, 0));

    mclfunc_getbody(cstate->func)[prep_pc] =
            CREATE_ABx(OP_TFORPREP, base, (u32) (call_pc - (prep_pc + 1)));
    mclfunc_getbody(cstate->func)[loop_pc] =
            CREATE_ABx(OP_TFORLOOP, base, (u32) ((loop_pc + 1) - bodystart));

    for (idx = saved_breaks; idx < cstate->nbreaks; idx += 1) {
        mccomp_patch(cstate, cstate->breaks[idx], FUNCLEN(cstate));
    }
    cstate->nbreaks = saved_breaks;
    mccomp_emit(cstate, CREATE_ABCk(OP_CLOSE, base, 0, 0, 0));

    cstate->nactive = base;
    cstate->freereg = base;
    return 0;
}

static i8 mccomp_break(MCComp* cstate) {
    i64 pc = 0;
    if (0 == cstate->in_loop) {
        printf("[CC] break outside loop\n");
        return 0;
    }

    if (cstate->nbreaks >= cstate->config->MAX_BREAKS) {
        printf("[CC] too many breaks\n");
        return 0;
    }

    pc = mccomp_emit(cstate, CREATE_sJ(OP_JMP, 0));
    cstate->breaks[cstate->nbreaks] = (u32) pc;
    cstate->nbreaks += 1;
    return 0;
}

static i8 mccomp_funcdef(MCComp* cstate, heap_header* stat) {
    heap_header* name = AST_CHILD(stat, 0);
    heap_header* arglist = AST_CHILD(stat, 1);
    heap_header* block = AST_CHILD(stat, 2);
    u8 is_method = (u8) (0 != AST_VAL(stat).integer);
    u32 nparts = AST_NCHILD(name);
    heap_header* proto = NULL;
    i32 pidx = 0;
    i32 konst = 0;
    u8 reg = 0;
    u8 tbl = 0;
    u32 idx = 0;

    proto = mccomp_proto(cstate, AST_VAL(name).varname,
            arglist, block, is_method);
    if (NULL == proto) {
        cstate->error = 1;
        return -1;
    }

    pidx = mclfunc_add_proto(cstate->func, proto, cstate->heap);
    if (pidx < 0) {
        cstate->error = 1;
        return -1;
    }

    if (0 == nparts) {
        konst = mccomp_const_name(cstate, AST_VAL(name).varname);
        reg = mccomp_reserve(cstate);
        mccomp_emit(cstate, CREATE_ABx(OP_CLOSURE, reg, (u32) pidx));
        mccomp_emit(cstate, CREATE_ABCk(OP_SETTABUP, 0, (u8) konst, reg, 0));
        cstate->freereg = cstate->nactive;
        return 0;
    }

    konst = mccomp_const_name(cstate, AST_VAL(name).varname);
    tbl = mccomp_reserve(cstate);
    mccomp_emit(cstate, CREATE_ABCk(OP_GETTABUP, tbl, 0, (u8) konst, 0));
    for (idx = 0; idx + 1 < nparts; idx += 1) {
        konst = mccomp_const_name(cstate, AST_VAL(AST_CHILD(name, idx)).varname);
        mccomp_emit(cstate, CREATE_ABCk(OP_GETFIELD, tbl, tbl, (u8) konst, 0));
    }

    reg = mccomp_reserve(cstate);
    mccomp_emit(cstate, CREATE_ABx(OP_CLOSURE, reg, (u32) pidx));

    konst = mccomp_const_name(cstate, AST_VAL(AST_CHILD(name, (nparts - 1))).varname);
    mccomp_emit(cstate, CREATE_ABCk(OP_SETFIELD, tbl, (u8) konst, reg, 0));
    cstate->freereg = cstate->nactive;
    return 0;
}

static i8 mccomp_localfunc(MCComp* cstate, heap_header* stat) {
    heap_header* body = AST_CHILD(stat, 0);
    heap_header* proto = NULL;
    i32 pidx = 0;
    u8 reg = cstate->nactive;

    if (0 != mccomp_newlocal(cstate, AST_VAL(stat).varname, reg)) {
        return -1;
    }
    cstate->freereg = cstate->nactive;

    proto = mccomp_proto(cstate, AST_VAL(stat).varname,
            AST_CHILD(body, 0), AST_CHILD(body, 1), 0);
    if (NULL == proto) {
        cstate->error = 1;
        return -1;
    }

    pidx = mclfunc_add_proto(cstate->func, proto, cstate->heap);
    if (pidx < 0) {
        cstate->error = 1;
        return -1;
    }

    mccomp_emit(cstate, CREATE_ABx(OP_CLOSURE, reg, (u32) pidx));
    return 0;
}

static i32 mccomp_func_expr(MCComp* cstate, heap_header* node) {
    heap_header* body = AST_CHILD(node, 0);
    heap_header* proto = NULL;
    i32 pidx = 0;
    u8 reg = 0;

    proto = mccomp_proto(cstate, (char*) "anonymous",
            AST_CHILD(body, 0), AST_CHILD(body, 1), 0);
    if (NULL == proto) {
        cstate->error = 1;
        return -1;
    }

    pidx = mclfunc_add_proto(cstate->func, proto, cstate->heap);
    if (pidx < 0) {
        cstate->error = 1;
        return -1;
    }

    reg = mccomp_reserve(cstate);
    mccomp_emit(cstate, CREATE_ABx(OP_CLOSURE, reg, (u32) pidx));
    return reg;
}

static i8 mccomp_label(MCComp* cstate, heap_header* stat) {
    char* name = AST_VAL(stat).varname;
    u32 here = FUNCLEN(cstate);
    u8 idx = 0;

    if (cstate->nlabels >= 64) {
        printf("[CC] too many labels\n");
        cstate->error = 1;
        return -1;
    }

    cstate->labels[cstate->nlabels].name = name;
    cstate->labels[cstate->nlabels].pc = here;
    cstate->nlabels += 1;
    for (idx = 0; idx < cstate->ngotos; idx += 1) {
        if ((NULL != cstate->gotos[idx].name)
                && (cstate->gotos[idx].name == name)) {
            mccomp_patch(cstate, cstate->gotos[idx].pc, here);
            cstate->gotos[idx].name = NULL;
        }
    }
    return 0;
}

static i8 mccomp_goto(MCComp* cstate, heap_header* stat) {
    char* name = AST_VAL(stat).varname;
    i64 pc = 0;
    u8 idx = 0;

    pc = mccomp_emit(cstate, CREATE_sJ(OP_JMP, 0));
    for (idx = 0; idx < cstate->nlabels; idx += 1) {
        if (cstate->labels[idx].name == name) {
            mccomp_patch(cstate, pc, cstate->labels[idx].pc);
            return 0;
        }
    }

    if (cstate->ngotos >= 64) {
        printf("[CC] too many pending gotos\n");
        cstate->error = 1;
        return -1;
    }

    cstate->gotos[cstate->ngotos].name = name;
    cstate->gotos[cstate->ngotos].pc = pc;
    cstate->ngotos += 1;
    return 0;
}

static i8 mccomp_stat(MCComp* cstate, heap_header* stat) {
    switch (AST_TYPE(stat)) {
        case PN_LOCAL:
            return mccomp_local(cstate, stat);
        case PN_ASSIGN:
            return mccomp_assign(cstate, stat);
        case PN_RETURN:
            return mccomp_return(cstate, stat);
        case PN_IF:
            return mccomp_if(cstate, stat);
        case PN_WHILE:
            return mccomp_while(cstate, stat);
        case PN_REPEAT:
            return mccomp_repeat(cstate, stat);
        case PN_FORNUM:
            return mccomp_fornum(cstate, stat);
        case PN_DO:
            return mccomp_block(cstate, AST_CHILD(stat, 0));
        case PN_BREAK:
            return mccomp_break(cstate);
        case PN_CALL_STAT:


            return (mccomp_call(cstate, AST_CHILD(stat, 0), 0) < 0) ? -1 : 0;
        case PN_FUNCDEF:
            return mccomp_funcdef(cstate, stat);
        case PN_LOCAL_FUNC:
            return mccomp_localfunc(cstate, stat);
        case PN_FORIN:
            return mccomp_forin(cstate, stat);
        case PN_GOTO:
            return mccomp_goto(cstate, stat);
        case PN_LABEL:
            return mccomp_label(cstate, stat);
        default:
            printf("[CC] unsupported statement (%d)\n", AST_TYPE(stat));
            return 0;
    }
}

static i8 mccomp_block_body(MCComp* cstate, heap_header* block) {
    u32 nchild = AST_NCHILD(block);
    u32 idx = 0;

    for (idx = 0; idx < nchild; idx += 1) {
        if (0 != mccomp_stat(cstate, AST_CHILD(block, idx))) {
            return -1;
        }




        cstate->freereg = cstate->nactive;
    }

    return 0;
}

static i8 mccomp_block(MCComp* cstate, heap_header* block) {
    u8 saved = cstate->nactive;
    i8 rcode = mccomp_block_body(cstate, block);

    cstate->nactive = saved;
    cstate->freereg = saved;

    return rcode;
}

static heap_header* mccomp_body(MCComp* cstate, char* name,
        heap_header* arglist, heap_header* block, u8 is_method) {
    mclfunc* fn = NULL;
    u32 idx = 0;

    cstate->func = mclfunc_alloc(INITIAL_CODE_CAP, cstate->heap);
    if (NULL == cstate->func) {
        printf("[CC] failed to allocate function object\n");
        return NULL;
    }

    fn = cstate->func->object.function;
    fn->name = name;
    fn->is_vararg = (u8) (0 != AST_VAL(arglist).integer);

    {
        char* envname = mcstrtbl_intern(cstate->strtbl, "_ENV", 4);
        if (NULL == cstate->parent) {
            mclfunc_add_upval(cstate->func, envname, 1, 0, cstate->heap);
        } else {
            mclfunc_add_upval(cstate->func, envname, 0, 0, cstate->heap);
        }
    }

    cstate->nactive = 0;
    cstate->freereg = 0;
    if (is_method) {
        if (0 != mccomp_newlocal(cstate,
                mcstrtbl_intern(cstate->strtbl, "self", 4), 0)) {
            return NULL;
        }
    }

    for (idx = 0; idx < AST_NCHILD(arglist); idx += 1) {
        if (0 != mccomp_newlocal(cstate, AST_VAL(AST_CHILD(arglist, idx)).varname,
                cstate->nactive)) {
            return NULL;
        }
    }

    fn->num_params = cstate->nactive;
    cstate->freereg = cstate->nactive;
    cstate->maxreg = cstate->freereg;
    if (fn->is_vararg) {
        mccomp_emit(cstate, CREATE_ABCk(OP_VARARGPREP, fn->num_params, 0, 0, 0));
    }

    if (0 != mccomp_block(cstate, block)) {
        return NULL;
    }

    fn = cstate->func->object.function;
    if (cstate->has_tbc || cstate->has_capture || fn->is_vararg) {
        mccomp_emit(cstate, CREATE_ABCk(OP_RETURN, cstate->nactive, 1,
                (u8) (fn->is_vararg ? (fn->num_params + 1) : 0),
                (cstate->has_tbc || cstate->has_capture) ? 1 : 0));
    } else {
        mccomp_emit(cstate, CREATE_ABCk(OP_RETURN0, cstate->nactive, 0, 0, 0));
    }

    fn = cstate->func->object.function;
    fn->max_stack = (cstate->maxreg < 2) ? 2 : cstate->maxreg;
    if (cstate->error) {
        return NULL;
    }
    return cstate->func;
}

static heap_header* mccomp_proto(MCComp* parent, char* name,
        heap_header* arglist, heap_header* block, u8 is_method) {
    MCComp sub;

    if (0 != mccomp_init(&sub, parent->heap, parent->config, parent->strtbl)) {
        return NULL;
    }
    sub.parent = parent;
    return mccomp_body(&sub, name, arglist, block, is_method);
}

i8 mccomp_init(MCComp* cstate, MCHeap* heap, VMConfig* config, StringTable* strtbl) {
    LocalVar* locals = NULL;
    u32* breaks = NULL;

    if ((NULL == cstate) || (NULL == heap) || (NULL == config) || (NULL == strtbl)) {
        return -1;
    }

    locals = (LocalVar*) mcheap_static_reserve(heap,
            config->MAX_LOCALS * sizeof(LocalVar));
    breaks = (u32*) mcheap_static_reserve(heap,
            config->MAX_BREAKS * sizeof(u32));
    if ((NULL == locals) || (NULL == breaks)) {
        return -1;
    }

    cstate->heap = heap;
    cstate->config = config;
    cstate->strtbl = strtbl;
    cstate->parent = NULL;
    cstate->func = NULL;
    cstate->locals = locals;
    cstate->nactive = 0;
    cstate->freereg = 0;
    cstate->maxreg = 0;
    cstate->breaks = breaks;
    cstate->nbreaks = 0;
    cstate->in_loop = 0;
    cstate->error = 0;
    cstate->has_tbc = 0;
    cstate->has_capture = 0;
    cstate->nlabels = 0;
    cstate->ngotos = 0;

    return 0;
}

heap_header* mccomp_run(MCComp* cstate, heap_header* ast_root) {
    if ((NULL == cstate) || (NULL == ast_root)) {
        return NULL;
    }
    if (PN_FUNCDEF != AST_TYPE(ast_root)) {
        printf("[CC] root node is not a function definition\n");
        return NULL;
    }
    return mccomp_body(cstate, AST_VAL(AST_CHILD(ast_root, 0)).varname,
            AST_CHILD(ast_root, 1), AST_CHILD(ast_root, 2), 0);
}

static void mccomp_log_instr(u32 instr) {
    enum OpCode op = GET_OPCODE(instr);

    printf("%-10s", mclop_names[op]);
    switch (GetOpMode(op)) {
        case iABC:
            printf("\t%d %d %d", GETARG_A(instr), GETARG_B(instr), GETARG_C(instr));
            if (GETARG_k(instr)) {
                printf(" k");
            }
            break;
        case iABx:
            printf("\t%d %u", GETARG_A(instr), GETARG_Bx(instr));
            break;
        case iAsBx:
            printf("\t%d %d", GETARG_A(instr), GETARG_sBx(instr));
            break;
        case iAx:
            printf("\t%u", GETARG_Ax(instr));
            break;
        case isJ:
            printf("\t%d", GETARG_sJ(instr));
            break;
        default:
            break;
    }
    printf("\n");
}

static void mccomp_log_consts(heap_header* func) {
    mclfunc* fn = func->object.function;
    Constant* consts = mclfunc_getconsts(func);
    u32 idx = 0;

    if (0 == fn->num_consts) {
        return;
    }
    printf("constants (%u):\n", fn->num_consts);
    for (idx = 0; idx < fn->num_consts; idx += 1) {
        printf("\t%u\t", idx);
        switch (consts[idx].type) {
            case K_INT:
                printf("int\t%ld\n", consts[idx].val.integer);
                break;
            case K_FLT:
                printf("flt\t%f\n", consts[idx].val.number);
                break;
            case K_STR:
                printf("str\t%s\n", mclstr_getchars(consts[idx].val.heapobj));
                break;
        }
    }
}

void mccomp_log(heap_header* func) {
    mclfunc* fn = NULL;
    heap_header** protos = NULL;
    u32* code = NULL;
    u32 idx = 0;

    if ((NULL == func) || (OBJ_FUNC != func->type)) {
        return;
    }

    fn = func->object.function;
    code = mclfunc_getbody(func);
    printf("function <%s> (%u instructions, %u slots)\n",
            (NULL != fn->name) ? fn->name : "?", fn->code_len, fn->max_stack);
    printf("%u params, %s, %u upvals\n",
            fn->num_params, fn->is_vararg ? "vararg" : "fixed", fn->num_upvals);

    for (idx = 0; idx < fn->code_len; idx += 1) {
        printf("\t%u\t", idx + 1);
        mccomp_log_instr(code[idx]);
    }
    mccomp_log_consts(func);

    protos = mclfunc_getprotos(func);
    for (idx = 0; idx < fn->num_protos; idx += 1) {
        printf("\n");
        mccomp_log(protos[idx]);
    }
}
