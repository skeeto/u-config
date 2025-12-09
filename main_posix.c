// POSIX platform layer for u-config
// This is free and unencumbered software released into the public domain.
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include "src/u-config.c"

#ifndef PKG_CONFIG_SYSTEM_INCLUDE_PATH
#  define PKG_CONFIG_SYSTEM_INCLUDE_PATH "/usr/include"
#endif

#ifndef PKG_CONFIG_SYSTEM_LIBRARY_PATH
#  define PKG_CONFIG_SYSTEM_LIBRARY_PATH "/lib:/usr/lib"
#endif

#ifndef PKG_CONFIG_PREFIX
#  define PKG_CONFIG_PREFIX "/usr"
#endif

static const char pkg_config_path[] =
    #ifdef PKG_CONFIG_PATH
    PKG_CONFIG_PATH ":"
    #endif
    #ifdef PKG_CONFIG_LIBDIR
    PKG_CONFIG_LIBDIR
    #else
    PKG_CONFIG_PREFIX "/local/lib/pkgconfig:"
    PKG_CONFIG_PREFIX "/local/share/pkgconfig:"
    PKG_CONFIG_PREFIX "/lib/pkgconfig:"
    PKG_CONFIG_PREFIX "/share/pkgconfig"
    #endif
;

static void os_fail(os *ctx)
{
    (void)ctx;
    _exit(1);
}

static void os_write(os *ctx, i32 fd, s8 s)
{
    (void)ctx;
    assert(fd==1 || fd==2);
    while (s.len) {
        ssize_t r = write(fd, s.s, (size_t)s.len);
        if (r < 0) {
            _exit(1);
        }
        s = cuthead(s, (iz)r);
    }
}

static filemap os_mapfile(os *ctx, arena *perm, s8 path)
{
    (void)ctx;
    assert(path.s);
    assert(path.len);
    assert(!path.s[path.len-1]);

    filemap r = {0};

    int fd = open((char *)path.s, 0);
    if (fd < 0) {
        r.status = filemap_NOTFOUND;
        return r;
    }

    r.data.s = (u8 *)perm->beg;
    iz cap = perm->end - perm->beg;
    while (r.data.len < cap) {
        u8 *dst = r.data.s + r.data.len;
        ssize_t len = read(fd, dst, (size_t)(cap-r.data.len));
        if (len < 1) {
            break;
        }
        r.data.len += len;
    }
    close(fd);

    if (r.data.len == cap) {
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
    assert(path.s);
    assert(path.len);
    assert(!path.s[path.len-1]);

    // NOTE: will allocate while holding this handle
    DIR *handle = opendir((char *)path.s);
    if (!handle) {
        return 0;
    }

    s8list files = {0};
    for (struct dirent *d; (d = readdir(handle));) {
        s8 name = s8fromcstr((u8 *)d->d_name);
        if (endswith(name, S(".pc"))) {
            s8 copy = news8(a, name.len);
            s8copy(copy, name);
            append(&files, copy, a);
        }
    }

    closedir(handle);
    return files.head;
}

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
    conf->haslisting = 1;
    return conf;
}

static s8 s8getenv_(char *k)
{
    return s8fromcstr((u8 *)getenv(k));
}

int main(int argc, char **argv)
{
    config *conf = newconfig_();
    conf->delim = ':';

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
    conf->sysrootdir   = s8getenv_("PKG_CONFIG_SYSROOT_DIR");
    conf->print_sysinc = s8getenv_("PKG_CONFIG_ALLOW_SYSTEM_CFLAGS");
    conf->print_syslib = s8getenv_("PKG_CONFIG_ALLOW_SYSTEM_LIBS");

    uconfig(conf);
    return 0;
}
