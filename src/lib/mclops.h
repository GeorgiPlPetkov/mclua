#pragma once

#include "mctypes.h"

enum OpCode {
	OP_MOVE, OP_LOADI, OP_LOADF, OP_LOADK, OP_LOADKX,
	OP_LOADFALSE, OP_LFALSESKIP, OP_LOADTRUE, OP_LOADNIL,
	OP_GETUPVAL, OP_SETUPVAL,
	OP_GETTABUP, OP_GETTABLE, OP_GETI, OP_GETFIELD,
	OP_SETTABUP, OP_SETTABLE, OP_SETI, OP_SETFIELD,
	OP_NEWTABLE, OP_SELF,
	OP_ADDI, OP_ADDK, OP_SUBK, OP_MULK, OP_MODK, OP_POWK, OP_DIVK, OP_IDIVK,
	OP_BANDK, OP_BORK, OP_BXORK,
	OP_SHRI, OP_SHLI,
	OP_ADD, OP_SUB, OP_MUL, OP_MOD, OP_POW, OP_DIV, OP_IDIV,
	OP_BAND, OP_BOR, OP_BXOR, OP_SHL, OP_SHR,
	OP_MMBIN, OP_MMBINI, OP_MMBINK,
	OP_UNM, OP_BNOT, OP_NOT, OP_LEN,
	OP_CONCAT,
	OP_CLOSE, OP_TBC, OP_JMP,
	OP_EQ, OP_LT, OP_LE,
	OP_EQK, OP_EQI, OP_LTI, OP_LEI, OP_GTI, OP_GEI,
	OP_TEST, OP_TESTSET,
	OP_CALL, OP_TAILCALL,
	OP_RETURN, OP_RETURN0, OP_RETURN1,
	OP_FORLOOP, OP_FORPREP, OP_TFORPREP, OP_TFORCALL, OP_TFORLOOP,
	OP_SETLIST, OP_CLOSURE,
	OP_VARARG, OP_VARARGPREP,
	OP_EXTRAARG, NUM_OPCODES
};

enum OpMode { iABC, iABx, iAsBx, iAx, isJ };

#define Opmode(is_mm, is_call, uses_k, is_test, sets_a, format) \
	(((is_mm) << 7) | ((is_call) << 6) | ((uses_k) << 5) | ((is_test) << 4) | ((sets_a) << 3) | (format))

#define GetOpMode(op)   ((enum OpMode) (mclop_modes[op] & 0x7))
#define TestAMode(op)   (mclop_modes[op] & (1 << 3))
#define TestTMode(op)   (mclop_modes[op] & (1 << 4))
#define TestITMode(op)  (mclop_modes[op] & (1 << 5))
#define TestOTMode(op)  (mclop_modes[op] & (1 << 6))
#define TestMMMode(op)  (mclop_modes[op] & (1 << 7))

static const char* mclop_names[NUM_OPCODES] = {
	"MOVE", "LOADI", "LOADF", "LOADK", "LOADKX",
	"LOADFALSE", "LFALSESKIP", "LOADTRUE", "LOADNIL",
	"GETUPVAL", "SETUPVAL",
	"GETTABUP", "GETTABLE", "GETI", "GETFIELD",
	"SETTABUP", "SETTABLE", "SETI", "SETFIELD",
	"NEWTABLE", "SELF",
	"ADDI", "ADDK", "SUBK", "MULK", "MODK", "POWK", "DIVK", "IDIVK",
	"BANDK", "BORK", "BXORK",
	"SHRI", "SHLI",
	"ADD", "SUB", "MUL", "MOD", "POW", "DIV", "IDIV",
	"BAND", "BOR", "BXOR", "SHL", "SHR",
	"MMBIN", "MMBINI", "MMBINK",
	"UNM", "BNOT", "NOT", "LEN",
	"CONCAT",
	"CLOSE", "TBC", "JMP",
	"EQ", "LT", "LE",
	"EQK", "EQI", "LTI", "LEI", "GTI", "GEI",
	"TEST", "TESTSET",
	"CALL", "TAILCALL",
	"RETURN", "RETURN0", "RETURN1",
	"FORLOOP", "FORPREP", "TFORPREP", "TFORCALL", "TFORLOOP",
	"SETLIST", "CLOSURE",
	"VARARG", "VARARGPREP",
	"EXTRAARG",
};

static const u8 mclop_modes[NUM_OPCODES] = {
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iAsBx),
	Opmode(0, 0, 0, 0, 1, iAsBx),
	Opmode(0, 0, 0, 0, 1, iABx),
	Opmode(0, 0, 0, 0, 1, iABx),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 0, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 0, iABC),
	Opmode(0, 0, 0, 0, 0, iABC),
	Opmode(0, 0, 0, 0, 0, iABC),
	Opmode(0, 0, 0, 0, 0, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(1, 0, 0, 0, 0, iABC),
	Opmode(1, 0, 0, 0, 0, iABC),
	Opmode(1, 0, 0, 0, 0, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 0, iABC),
	Opmode(0, 0, 0, 0, 0, iABC),
	Opmode(0, 0, 0, 0, 0, isJ),
	Opmode(0, 0, 1, 1, 0, iABC),
	Opmode(0, 0, 1, 1, 0, iABC),
	Opmode(0, 0, 1, 1, 0, iABC),
	Opmode(0, 0, 1, 1, 0, iABC),
	Opmode(0, 0, 1, 1, 0, iABC),
	Opmode(0, 0, 1, 1, 0, iABC),
	Opmode(0, 0, 1, 1, 0, iABC),
	Opmode(0, 0, 1, 1, 0, iABC),
	Opmode(0, 0, 1, 1, 0, iABC),
	Opmode(0, 0, 0, 1, 0, iABC),
	Opmode(0, 0, 0, 1, 1, iABC),
	Opmode(0, 1, 0, 0, 1, iABC),
	Opmode(0, 1, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 0, iABC),
	Opmode(0, 0, 0, 0, 0, iABC),
	Opmode(0, 0, 0, 0, 0, iABC),
	Opmode(0, 0, 0, 0, 1, iABx),
	Opmode(0, 0, 0, 0, 1, iABx),
	Opmode(0, 0, 0, 0, 0, iABx),
	Opmode(0, 0, 0, 0, 0, iABC),
	Opmode(0, 0, 0, 0, 1, iABx),
	Opmode(0, 0, 1, 0, 0, iABC),
	Opmode(0, 0, 0, 0, 1, iABx),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 1, iABC),
	Opmode(0, 0, 0, 0, 0, iAx),
};

#define SIZE_OP (7)
#define POS_OP  (0)
#define POS_A   (POS_OP + SIZE_OP)
#define POS_k   (POS_A + 8)
#define POS_B   (POS_k + 1)
#define POS_C   (POS_B + 8)
#define POS_Bx  (POS_k)
#define POS_Ax  (POS_A)
#define POS_sJ  (POS_A)

#define MAXARG_Bx  ((1 << 17) - 1)
#define OFFSET_sBx (MAXARG_Bx >> 1)
#define MAXARG_sJ  ((1 << 25) - 1)
#define OFFSET_sJ  (MAXARG_sJ >> 1)

#define CREATE_ABCk(o, a, b, c, k) \
	(((u32) (o)) | ((u32) (a) << POS_A) | ((u32) (k) << POS_k) \
	| ((u32) (b) << POS_B) | ((u32) (c) << POS_C))

#define CREATE_ABx(o, a, bx) \
		(((u32) (o)) | ((u32) (a) << POS_A) | ((u32) (bx) << POS_Bx))
#define CREATE_AsBx(o, a, sbx) CREATE_ABx((o), (a), (u32) ((sbx) + OFFSET_sBx))
#define CREATE_Ax(o, ax) (((u32) (o)) | ((u32) (ax) << POS_Ax))
#define CREATE_sJ(o, sj) (((u32) (o)) | ((u32) ((sj) + OFFSET_sJ) << POS_sJ))

#define GET_OPCODE(i) ((enum OpCode) ((i) & 0x7F))
#define GETARG_A(i)   (((i) >> POS_A) & 0xFF)
#define GETARG_B(i)   (((i) >> POS_B) & 0xFF)
#define GETARG_C(i)   (((i) >> POS_C) & 0xFF)
#define GETARG_k(i)   (((i) >> POS_k) & 0x1)
#define GETARG_Bx(i)  (((i) >> POS_Bx) & MAXARG_Bx)
#define GETARG_sBx(i) ((i32) GETARG_Bx(i) - OFFSET_sBx)
#define GETARG_Ax(i)  ((i) >> POS_Ax)
#define GETARG_sJ(i)  ((i32) (((i) >> POS_sJ) & MAXARG_sJ) - OFFSET_sJ)
