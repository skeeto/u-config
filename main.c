// Generic C platform layer for u-config
// This is free and unencumbered software released into the public domain.
#define _CRT_SECURE_NO_WARNINGS
#ifndef __GNUC__
#  define __builtin_unreachable()  *(volatile int *)0 = 0
#  define __attribute(x)
#endif
#include "src/u-config.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static arena newarena_(void)
{
    iz cap = (iz)1<<22;
    arena a = {0};
    a.beg = malloc((size_t)cap);
    if (!a.beg) {
        a.beg = (byte *)16;  // aligned, non-null, zero-size arena
        cap = 0;
    }
    a.end = a.beg + cap;
    return a;
}

static config *newconfig_(void)
{
    arena perm = newarena_();
    config *conf = new(&perm, config, 1);
    conf->perm = perm;
    return conf;
}

static s8 s8getenv_(char *k)
{
    return s8fromcstr((u8 *)getenv(k));
}

int main(int argc, char **argv)
{
    config *conf = newconfig_();
    conf->delim = (u8)PATHDELIM[0];
    conf->define_prefix = PKG_CONFIG_DEFINE_PREFIX;

    if (argc) {
        argc--;
        argv++;
    }
    conf->args = (u8 **)argv;
    conf->nargs = argc;

    conf->pc_path = S(pkg_config_path);
    conf->pc_sysincpath = S(PKG_CONFIG_SYSTEM_INCLUDE_PATH);
    conf->pc_syslibpath = S(PKG_CONFIG_SYSTEM_LIBRARY_PATH);
    conf->envpath = s8getenv_("PKG_CONFIG_PATH");
    conf->fixedpath = s8getenv_("PKG_CONFIG_LIBDIR");
    if (!conf->fixedpath.s) {
        conf->fixedpath = S(pkg_config_path);
    }
    conf->sys_incpath = s8getenv_("PKG_CONFIG_SYSTEM_INCLUDE_PATH");
    if (!conf->sys_incpath.s) {
        conf->sys_incpath = S(PKG_CONFIG_SYSTEM_INCLUDE_PATH);
    }
    conf->sys_libpath = s8getenv_("PKG_CONFIG_SYSTEM_LIBRARY_PATH");
    if (!conf->sys_libpath.s) {
        conf->sys_libpath = S(PKG_CONFIG_SYSTEM_LIBRARY_PATH);
    }
    conf->top_builddir = s8getenv_("PKG_CONFIG_TOP_BUILD_DIR");
    conf->print_sysinc = s8getenv_("PKG_CONFIG_ALLOW_SYSTEM_CFLAGS");
    conf->print_syslib = s8getenv_("PKG_CONFIG_ALLOW_SYSTEM_LIBS");

    uconfig(conf);

    return ferror(stdout);
}

static filemap os_mapfile(os *ctx, arena *perm, s8 path)
{
    (void)ctx;
    assert(path.len > 0);
    assert(!path.s[path.len-1]);

    filemap r = {0};

    FILE *f = fopen((char *)path.s, "rb");
    if (!f) {
        r.status = filemap_NOTFOUND;
        return r;
    }

    r.data.s = (u8 *)perm->beg;
    iz available = perm->end - perm->beg;
    r.data.len = (iz)fread(r.data.s, 1, (size_t)available, f);
    fclose(f);

    if (r.data.len == available) {
        // If it filled all available space, assume the file is too large
        r.status = filemap_READERR;
        return r;
    }

    perm->beg += r.data.len;
    r.status = filemap_OK;
    return r;
}

static s8node *os_listing(os *ctx, arena *a, s8 path)
{
    (void)ctx;
    (void)a;
    (void)path;
    return 0;
}

static void os_write(os *ctx, i32 fd, s8 s)
{
    (void)ctx;
    assert(fd==1 || fd==2);
    FILE *f = fd==1 ? stdout : stderr;
    fwrite(s.s, (size_t)s.len, 1, f);
    fflush(f);
}

static void os_fail(os *ctx)
{
    (void)ctx;
    exit(1);
}
