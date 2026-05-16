#include "mcmemops.h"

#include "mcfs.h"

i8 mcfs_openfile(mcfile* file, const char* path, const char* opts) {
    file->fhandle = fopen(path, opts);

    if (NULL == file->fhandle) {
        return -1;
    }

    Memcopy(file->opts, opts, 4);

    return 0;
}

i8 mcfs_loadas_str8(str8* str, const char* path) {
    i8 rcode = 0;
    u64 filesize = 0;
    u64 readcnt = 0;
    FILE* file = fopen(path, "r");

    if (NULL == file) {
        rcode = -1;
        return rcode;
    }

    fseek(file, 0, SEEK_END);
    filesize = ftell(file);
    rewind(file);

    if ((0 > filesize) || (str->capacity < (filesize + 1))) {
        rcode = -1;
        goto FEXIT;
    }

    while (filesize > readcnt) {
        readcnt += fread(
            (str->content),
            1,
            (filesize - readcnt),
            file
        );
    }

FEXIT:
    fclose(file);

    return rcode;
}

void mcfs_closefile(mcfile* file);
