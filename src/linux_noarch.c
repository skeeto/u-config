// libc-free platform layer for Linux (arch-agnostic parts)
// This is free and unencumbered software released into the public domain.

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
// macro definitions for SYS_read, SYS_write, SYS_open, SYS_close,
// SYS_exit. Arch-specific _start calls arch-agnostic entrypoint() with
// the process entry stack pointer.
static long syscall1(long, long);
static long syscall2(long, long, long);
static long syscall3(long, long, long, long);

static void os_fail(void)
{
    syscall1(SYS_exit, 1);
    __builtin_unreachable();
}

static void os_write(i32 fd, s8 s)
{
    assert(fd==1 || fd==2);
    while (s.len) {
        long r = syscall3(SYS_write, fd, (long)s.s, s.len);
        if (r < 0) {
            os_fail();
        }
        s = cuthead(s, (size)r);
    }
}

static filemap os_mapfile(arena *perm, s8 path)
{
    assert(path.s);
    assert(path.len);
    assert(!path.s[path.len-1]);

    filemap r = {0};

    long fd = syscall2(SYS_open, (long)path.s, 0);
    if (fd < 0) {
        r.status = filemap_NOTFOUND;
        return r;
    }

    r.data.s = (u8 *)perm->beg;
    size cap = perm->end - perm->beg;
    while (r.data.len < cap) {
        u8 *dst = r.data.s + r.data.len;
        long len = syscall3(SYS_read, fd, (long)dst, cap-r.data.len);
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

static s8 fromcstr_(u8 *z)
{
    s8 s = {0};
    s.s = (u8 *)z;
    if (s.s) {
        for (; s.s[s.len]; s.len++) {}
    }
    return s;
}

static arena getarena_(void)
{
    static byte heap[1<<22];
    byte *mem = heap;
    asm ("" : "+r"(mem));  // launder
    return (arena){mem, mem+countof(heap)};
}

static config *newconfig_(void)
{
    arena perm = getarena_();
    config *conf = new(&perm, config, 1);
    conf->perm = perm;
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
    conf->args = new(&conf->perm, s8, argc);
    for (i32 i = 0; i < argc; i++) {
        conf->args[i] = fromcstr_(argv[i]);
    }

    conf->pc_path = S(PKG_CONFIG_LIBDIR);
    conf->pc_sysincpath = S(PKG_CONFIG_SYSTEM_INCLUDE_PATH);
    conf->pc_syslibpath = S(PKG_CONFIG_SYSTEM_LIBRARY_PATH);
    conf->fixedpath = S(PKG_CONFIG_LIBDIR);
    conf->sys_incpath = S(PKG_CONFIG_SYSTEM_INCLUDE_PATH);
    conf->sys_libpath = S(PKG_CONFIG_SYSTEM_LIBRARY_PATH);

    for (u8 **v = envp; *v; v++) {
        cut c = s8cut(fromcstr_(*v), '=');
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

void entrypoint(long *stack)
{
    i32  argc   = (i32)stack[0];
    u8 **argv   = (u8 **)stack + 1;
    u8 **envp   = argv + argc + 1;
    i32  status = os_main(argc, argv, envp);
    syscall1(SYS_exit, status);
    __builtin_unreachable();
}
