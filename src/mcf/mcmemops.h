#pragma once

#include <string.h>
#include "mctypes.h"

#define MemsetZero(ptr, size) memset((ptr), 0, (size))
#define MemsetZeroTyped(ptr, typesize)                 \
        MemsetZero((ptr), sizeof(*(ptr)) * (typesize))
#define MemsetZeroStruct(ptr) MemsetZero((ptr), sizeof(*(ptr)))
#define MemsetZeroArray(ptr) MemsetZero((ptr), sizeof(ptr))

#define Memcopy(dest, src, size) memmove((dest), (src), (size))
#define MemcopyStruct(dest, src) Memcopy((dest), (src), \
        MinI(sizeof(*(dest)), sizeof(*(src))))
#define MemcopyArray(dest, src) Memcopy((dest), (src), \
        MinI(sizeof(src), sizeof(dest)))
#define MemcopyTyped(dest, src, typesize) Memcopy((dest), (src), \
        MinI(sizeof(*(dest)), sizeof(*(src))) * (typesize))
