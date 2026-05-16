#pragma once

#include <stdio.h>

#include "mctypes.h"
#include "mcstr.h"

typedef struct {
    FILE* fhandle;
    char  opts[4];
} mcfile;

i8 mcfs_openfile(mcfile* file, const char* path, const char* opts);
void mcfs_closefile(mcfile* file);

i8 mcfs_loadas_str8(str8* str, const char* path);

i8 mcfs_readfile(mcbfr* bfr, str8* path);
i8 mcfs_readblob(mcbfr* bfr, mcfile* file);
i8 mcfs_readline(mcbfr* bfr, mcfile* file);
i8 mcfs_readchar(mcbfr* bfr, mcfile* file);
