// Generic C platform layer for u-config
// This is free and unencumbered software released into the public domain.
#include "u-config.c"

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>

#if _WIN32
#  define PATHDELIM ";"
#  ifndef PKG_CONFIG_DEFINE_PREFIX
#    define PKG_CONFIG_DEFINE_PREFIX 1
#  endif
#else
#  define PATHDELIM ":"
#  ifndef PKG_CONFIG_DEFINE_PREFIX
#    define PKG_CONFIG_DEFINE_PREFIX 0
#  endif
#endif

#ifndef PKG_CONFIG_SYSTEM_INCLUDE_PATH
#  define PKG_CONFIG_SYSTEM_INCLUDE_PATH "/usr/include"
#endif

#ifndef PKG_CONFIG_SYSTEM_LIBRARY_PATH
#  define PKG_CONFIG_SYSTEM_LIBRARY_PATH "/lib" PATHDELIM "/usr/lib"
#endif

#ifndef PKG_CONFIG_PREFIX
#  define PKG_CONFIG_PREFIX "/usr"
#endif

static const char pkg_config_path[] =
    #ifdef PKG_CONFIG_PATH
    PKG_CONFIG_PATH PATHDELIM
    #endif
    #ifdef PKG_CONFIG_LIBDIR
    PKG_CONFIG_LIBDIR
    #else
    PKG_CONFIG_PREFIX "/local/lib/pkgconfig" PATHDELIM
    PKG_CONFIG_PREFIX "/local/share/pkgconfig" PATHDELIM
    PKG_CONFIG_PREFIX "/lib/pkgconfig" PATHDELIM
    PKG_CONFIG_PREFIX "/share/pkgconfig"
    #endif
;

static Str fromcstr_(char *z)
{
    Str s = {(Byte *)z, 0};
    if (s.s) {
        for (; s.s[s.len]; s.len++) {}
    }
    return s;
}

static Arena newarena_(void)
{
    // 16-bit    : allocate  16 KiB
    // 32/64-bit : allocate 256 MiB
    int exp = 7 * SIZEOF(int);
    Size cap = (Size)1 << exp;
    #ifdef DEBUG
    // Reduce for fuzzing and faster debugging
    cap = (Size)1 << 21;
    #endif

    Arena arena = {0};
    arena.mem.s = (Byte *)malloc(cap);
    arena.mem.len = arena.mem.s ? cap : 0;
    shredfree(&arena);
    return arena;
}

int main(int argc, char **argv)
{
    Config conf = {0};
    conf.delim = PATHDELIM[0];
    conf.define_prefix = PKG_CONFIG_DEFINE_PREFIX;
    conf.arena = newarena_();

    if (argc) {
        argc--;
        argv++;
    }
    conf.args = (Str *)allocarray(&conf.arena, SIZEOF(Str), argc);
    conf.nargs = argc;
    for (int i = 0; i < argc; i++) {
        conf.args[i] = fromcstr_(argv[i]);
    }

    conf.envpath = fromcstr_(getenv("PKG_CONFIG_PATH"));
    conf.fixedpath = fromcstr_(getenv("PKG_CONFIG_LIBDIR"));
    if (!conf.fixedpath.s) {
        conf.fixedpath = S(pkg_config_path);
    }
    conf.sys_incpath = fromcstr_(getenv("PKG_CONFIG_SYSTEM_INCLUDE_PATH"));
    if (!conf.sys_incpath.s) {
        conf.sys_incpath = S(PKG_CONFIG_SYSTEM_INCLUDE_PATH);
    }
    conf.sys_libpath = fromcstr_(getenv("PKG_CONFIG_SYSTEM_LIBRARY_PATH"));
    if (!conf.sys_libpath.s) {
        conf.sys_libpath = S(PKG_CONFIG_SYSTEM_LIBRARY_PATH);
    }
    conf.top_builddir  = fromcstr_(getenv("PKG_CONFIG_TOP_BUILD_DIR"));

    appmain(conf);

    #ifdef DEBUG
    free(conf.arena.mem.s);  // look ma, no memory leaks
    #endif
    return ferror(stdout);
}

static MapFileResult os_mapfile(Arena *a, Str path)
{
    ASSERT(path.len > 0);
    ASSERT(!path.s[path.len-1]);

    FILE *f = fopen((char *)path.s, "rb");
    if (!f) {
        MapFileResult r = {{0, 0}, MapFile_NOTFOUND};
        return r;
    }

    Arena tmp = *a;
    Str buf = maxstr(&tmp);
    Size len = (Size)fread(buf.s, 1, buf.len, f);
    fclose(f);

    if (len == buf.len) {
        // Assume file is too large
        MapFileResult readerr = {{0, 0}, MapFile_READERR};
        return readerr;
    }

    MapFileResult r = {newstr(a, len), MapFile_OK};
    return r;
}

static void os_write(int fd, Str s)
{
    ASSERT(fd==1 || fd==2);
    FILE *f = fd==1 ? stdout : stderr;
    fwrite(s.s, s.len, 1, f);
    fflush(f);
}

static void os_fail(void)
{
    exit(1);
}
