#pragma once

#ifdef _WIN32

#include <windows.h>

#define LIBOPEN(path)     ((void*) LoadLibraryA(path))
#define LIBSYM(lib, name) ((void*) (void (*)(void)) GetProcAddress((HMODULE) (lib), (name)))
#define LIBCLOSE(lib)     FreeLibrary((HMODULE) (lib))
#define LIBERR()          mcloadlib_lasterr()

static inline const char* mcloadlib_lasterr(void) {
    static char bfr[256];
    DWORD err = GetLastError();

    if (0 == err) {
        return "no error";
    }
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, err, 0, bfr, sizeof(bfr), NULL);
    return bfr;
}

#else

#include <dlfcn.h>

#define LIBOPEN(path)     dlopen((path), RTLD_NOW | RTLD_LOCAL)
#define LIBSYM(lib, name) dlsym((lib), (name))
#define LIBCLOSE(lib)     dlclose(lib)
#define LIBERR()          dlerror()

#endif
