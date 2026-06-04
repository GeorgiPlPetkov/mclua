#pragma once

#include "mctypes.h"
#include "mclast.h"
#include "mclfunc.h"
#include "mcheap.h"
#include "mcconfig.h"

typedef struct LocalVar {
    char* name;
    u8 reg;
    u8 attrib;          
} LocalVar;

typedef struct MCComp {
    MCHeap* heap;
    VMConfig* config;
    struct MCComp* parent;      
    heap_header* func;
    LocalVar* locals;
    u8 nactive;
    u8 freereg;
    u8 maxreg;
    u32* breaks;
    u8 nbreaks;
    u8 in_loop;
    u8 error;
    u8 has_tbc;                 
    u8 has_capture;             

    struct { char* name; u32 pc; } labels[64];
    u8 nlabels;
    struct { char* name; i64 pc; } gotos[64];
    u8 ngotos;
} MCComp;

i8 mccomp_init(MCComp* cstate, MCHeap* heap, VMConfig* config);
heap_header* mccomp_run(MCComp* cstate, heap_header* ast_root);

void mccomp_log(heap_header* func);
