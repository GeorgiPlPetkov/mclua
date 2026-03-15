#pragma once

#include <stdint.h>

// numeric types:

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float  f32;
typedef double f64;

// numeric limits:

#define MINI8  ((i8)  0x80)
#define MINI16 ((i16) 0x8000)
#define MINI32 ((i32) 0x80000000)
#define MINI64 ((i64) 0x8000000000000000llu)

#define MAXI8  ((i8)  0x7f)
#define MAXI16 ((i16) 0x7fff)
#define MAXI32 ((i32) 0x7fffffff)
#define MAXI64 ((i64) 0x7fffffffffffffffllu)

#define MAXU8  (0xff)
#define MAXU16 (0xffff)
#define MAXU32 (0xffffffff)
#define MAXU64 (0xffffffffffffffffllu)

#define MAXF32 (3.402823466e+38F)
#define MINF32 (1.175494351e-38F)
#define MAXF64 (1.7976931348623158e+308)
#define MINF64 (2.2250738585072014e-308)

// type operations:

#define MinI(i1, i2) (((i1) < (i2)) ? (i1) : (i2))
#define MaxI(i1, i2) (((i1) > (i2)) ? (i1) : (i2))
#define IsPow2(x) (((x) & ((x) - 1)) == 0)

typedef struct {
    u8* mem;
    u64 cap;
} mcbfr;
