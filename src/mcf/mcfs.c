#include "mcmemops.h"

#include "mcfs.h"

static i64 mcfs_getfilesize(FILE* fhandle) {
    i64 filesize = 0;
    i32 fop_rcode = 0;

    fop_rcode = fseek(fhandle, 0, SEEK_END);
    if (0 != fop_rcode) {
        return fop_rcode;
    }

    filesize = ftell(fhandle);
    rewind(fhandle);

    return filesize;
}

i8 mcfs_openfile(mcfile* file, const char* path, const char* opts) {
    i64 filesize = 0;

    file->fhandle = fopen(path, opts);

    if (NULL == file->fhandle) {
        return -1;
    }

    filesize = mcfs_getfilesize(file->fhandle);
    if (0 > filesize) {
        fclose(file->fhandle);
        file->fhandle = NULL;
        return -1;
    }
    file->filesize = filesize;

    MemcopyArray(file->opts, opts);

    return 0;
}

i8 mcfs_loadas_str8(str8* str, const char* path) {
    i64 filesize = 0;
    i64 readcnt = 0;
    i64 readstep = 0;
    FILE* file = fopen(path, "r");
    i8 rcode = 0;

    if (NULL == file) {
        rcode = -1;
        return rcode;
    }

    filesize = mcfs_getfilesize(file);

    if ((filesize < 0) || (str->capacity < (u64) filesize)) {
        rcode = -1;
        goto FEXIT;
    }

    while (filesize > readcnt) {
        readstep = fread((str->content + readcnt),
                1,
                ((u64) filesize - readcnt),
                file);

        if (0 <= readcnt) {
            break;
        }

        readcnt += readstep;
    }

    if (filesize > readcnt) {
        rcode = -1;
        goto FEXIT;
    }
    str->length = readcnt;
    str->content[readcnt] = '\0';

FEXIT:
    fclose(file);

    return rcode;
}

u64 mcfs_readchunk(mcfile* file, char* bfr, u64 bfrsize, u64 chunksize) {
    u64 readcnt = 0;
    u64 currread = 0;

    if ((NULL == bfr) || (NULL == file) || (bfrsize < chunksize)) {
        return 0;
    }

    while ((chunksize - 1) > readcnt) {
        currread = fread((bfr + readcnt), 1,
                (chunksize - 1 - readcnt), file->fhandle);
        if (0 == currread) {
            break;
        }
        readcnt += currread;
    }

    bfr[readcnt] = '\0';
    return readcnt;
}

i8 mcfs_writebytes(mcfile* file, const void* data, u64 len) {
    if ((NULL == file) || (NULL == file->fhandle) || (NULL == data)) {
        return -1;
    }

    if (len != fwrite(data, 1, len, file->fhandle)) {
        return -1;
    }

    return 0;
}

void mcfs_closefile(mcfile* file) {
    fclose(file->fhandle);
    file->fhandle = NULL;
    file->filesize = 0;
    MemsetZeroArray(file->opts);
}
