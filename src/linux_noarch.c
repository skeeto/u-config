// libc-free platform layer for Linux (arch-agnostic parts)
// This is free and unencumbered software released into the public domain.

#include "memory.c"

#ifndef PKG_CONFIG_LIBDIR
#  define PKG_CONFIG_LIBDIR \
     "/usr/local/lib/pkgconfig:" \
     "/usr/local/share/pkgconfig:" \
     "/usr/lib/pkgconfig:" \
     "/usr/share/pkgconfig"
#endif

#ifndef PKG_CONFIG_SYSTEM_INCLUDE_PATH
#  define PKG_CONFIG_SYSTEM_INCLUDE_PATH "/usr/include"
#endif

#ifndef PKG_CONFIG_SYSTEM_LIBRARY_PATH
#  define PKG_CONFIG_SYSTEM_LIBRARY_PATH "/lib:/usr/lib"
#endif

// Arch-specific definitions, defined by the includer. Also requires
// macro definitions for SYS_read, SYS_write, SYS_openat, SYS_close,
// SYS_exit. Arch-specific _start calls arch-agnostic entrypoint() with
// the process entry stack pointer.
static iz syscall1(iz, iz);
static iz syscall3(iz, iz, iz, iz);

enum {
    AT_FDCWD    = -100,
};

static void os_fail(os *ctx)
{
    (void)ctx;
    syscall1(SYS_exit, 1);
    __builtin_unreachable();
}

static void os_write(os *ctx, i32 fd, s8 s)
{
    (void)ctx;
    assert(fd==1 || fd==2);
    while (s.len) {
        iz r = syscall3(SYS_write, fd, (iz)s.s, s.len);
        if (r < 0) {
            os_fail(0);
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

    iz fd = syscall3(SYS_openat, AT_FDCWD, (iz)path.s, 0);
    if (fd < 0) {
        r.status = filemap_NOTFOUND;
        return r;
    }

    r.data.s = (u8 *)perm->beg;
    iz cap = perm->end - perm->beg;
    while (r.data.len < cap) {
        u8 *dst = r.data.s + r.data.len;
        iz len = syscall3(SYS_read, fd, (iz)dst, cap-r.data.len);
        if (len < 1) {
            break;
        }
        r.data.len += len;
    }
    syscall1(SYS_close, fd);

    if (r.data.len == cap) {
        // If it filled all available space, assume the file is too large
        r.status = filemap_READERR;
        return r;
    }

    perm->beg += r.data.len;
    r.status = filemap_OK;
    return r;
}

static b32 endswith_(s8 s, s8 suffix)
{
    return s.len>=suffix.len && s8equals(taketail(s, suffix.len), suffix);
}

static s8node *os_listing(os *ctx, arena *a, s8 path)
{
    (void)ctx;
    assert(path.s);
    assert(path.len);
    assert(!path.s[path.len-1]);

    // NOTE: will allocate while holding this file descriptor
    int fd = (int)syscall3(SYS_openat, AT_FDCWD, (iz)path.s, O_DIRECTORY);
    if (fd < 0) {
        return 0;
    }

    iz     cap   = 1<<14;
    byte  *buf   = new(a, byte, cap);
    s8list files = {0};
    for (;;) {
        iz len = syscall3(SYS_getdents64, fd, (iz)buf, cap);
        if (len < 1) {
            break;
        }

        for (iz off = 0; off < len;) {
            struct {
                long long ino;
                long long off;
                short     len;
                char      type;
                u8        name[];
            } *dents = (void *)(buf + off);
            off += dents->len;

            s8 name = s8fromcstr(dents->name);
            if (endswith_(name, S(".pc"))) {
                s8 copy = news8(a, name.len);
                s8copy(copy, name);
                append(&files, copy, a);
            }
        }
    }

    syscall1(SYS_close, fd);
    return files.head;
}

static arena getarena_(void)
{
    static byte heap[1<<22];
    byte *mem = heap;
    asm ("" : "+r"(mem));  // launder
    return (arena){mem, mem+countof(heap), 0};
}

static config *newconfig_(void)
{
    arena perm = getarena_();
    config *conf = new(&perm, config, 1);
    conf->perm = perm;
    conf->haslisting = 1;
    return conf;
}

static i32 os_main(i32 argc, u8 **argv, u8 **envp)
{
    config *conf = newconfig_();
    conf->delim = ':';

    if (argc) {
        argc--;
        argv++;
    }
    conf->nargs = argc;
    conf->args = argv;

    conf->pc_path = S(PKG_CONFIG_LIBDIR);
    conf->pc_sysincpath = S(PKG_CONFIG_SYSTEM_INCLUDE_PATH);
    conf->pc_syslibpath = S(PKG_CONFIG_SYSTEM_LIBRARY_PATH);
    conf->fixedpath = S(PKG_CONFIG_LIBDIR);
    conf->sys_incpath = S(PKG_CONFIG_SYSTEM_INCLUDE_PATH);
    conf->sys_libpath = S(PKG_CONFIG_SYSTEM_LIBRARY_PATH);

    for (u8 **v = envp; *v; v++) {
        cut c = s8cut(s8fromcstr(*v), '=');
        s8 name = c.head;
        s8 value = c.tail;

        if (s8equals(name, S("PKG_CONFIG_PATH"))) {
            conf->envpath = value;
        } else if (s8equals(name, S("PKG_CONFIG_LIBDIR"))) {
            conf->fixedpath = value;
        } else if (s8equals(name, S("PKG_CONFIG_TOP_BUILD_DIR"))) {
            conf->top_builddir = value;
        } else if (s8equals(name, S("PKG_CONFIG_SYSTEM_INCLUDE_PATH"))) {
            conf->sys_incpath = value;
        } else if (s8equals(name, S("PKG_CONFIG_SYSTEM_LIBRARY_PATH"))) {
            conf->sys_libpath = value;
        } else if (s8equals(name, S("PKG_CONFIG_ALLOW_SYSTEM_CFLAGS"))) {
            conf->print_sysinc = value;
        } else if (s8equals(name, S("PKG_CONFIG_ALLOW_SYSTEM_LIBS"))) {
            conf->print_syslib = value;
        }
    }

    uconfig(conf);
    return 0;
}

void entrypoint(iz *stack)
{
    i32  argc   = (i32)stack[0];
    u8 **argv   = (u8 **)stack + 1;
    u8 **envp   = argv + argc + 1;
    i32  status = os_main(argc, argv, envp);
    syscall1(SYS_exit, status);
    __builtin_unreachable();
}
