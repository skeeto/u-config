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
// macro definitions for SYS_write, SYS_open, SYS_close, SYS_fstat,
// SYS_mmap, SYS_exit. Arch-specific _start calls the arch-agnostic
// entrypoint() with the process entry stack pointer.
struct stat;  // only needs st_size
static long syscall1(long, long);
static long syscall2(long, long, long);
static long syscall3(long, long, long, long);
static long syscall6(long, long, long, long, long, long, long);

__attribute__((section(".text.memset")))
void *memset(void *d, int c, unsigned long n)
{
    char *dst = d;
    for (; n; n--) {
        *dst++ = (char)c;
    }
    return d;
}

__attribute__((section(".text.memcpy")))
void *memcpy(void *d, const void *s, unsigned long n)
{
    char *dst = d;
    for (const char *src = s; n; n--) {
        *dst++ = *src++;
    }
    return d;
}

static void os_fail(void)
{
    syscall1(SYS_exit, 1);
    __builtin_unreachable();
}

static void os_write(int fd, Str s)
{
    ASSERT(fd==1 || fd==2);
    while (s.len) {
        long r = syscall3(SYS_write, fd, (long)s.s, s.len);
        if (r < 0) {
            os_fail();
        }
        s = cuthead(s, (Size)r);
    }
}

static MapFileResult os_mapfile(Arena *a, Str path)
{
    (void)a;
    ASSERT(path.s);
    ASSERT(path.len);
    ASSERT(!path.s[path.len-1]);

    long fd = syscall2(SYS_open, (long)path.s, 0);
    if (fd < 0) {
        MapFileResult r = {.status=MapFile_NOTFOUND};
        return r;
    }

    struct stat sb;
    if (syscall2(SYS_fstat, fd, (long)&sb) < 0) {
        syscall1(SYS_close, fd);
        MapFileResult r = {.status=MapFile_READERR};
        return r;
    }

    if (sb.st_size > Size_MAX) {
        syscall1(SYS_close, fd);
        MapFileResult r = {.status=MapFile_READERR};
        return r;
    }
    Size size = (Size)sb.st_size;

    if (!size) {
        syscall1(SYS_close, fd);
        // Cannot map an empty file, so use the arena for a zero-size
        // allocation, distinguishing it from a null string.
        MapFileResult r = {newstr(a, 0), .status=MapFile_OK};
        return r;
    }

    unsigned long p = syscall6(SYS_mmap, 0, size, 1, 2, fd, 0);
    syscall1(SYS_close, fd);
    if (p > -4096UL) {
        MapFileResult r = {.status=MapFile_READERR};
        return r;
    }

    MapFileResult r = {{(Byte *)p, size}, .status=MapFile_OK};
    return r;
}

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
    Size size = 1 << 21;
    unsigned long p = syscall6(SYS_mmap, 0, size, 3, 0x22, -1, 0);
    if (p > -4096UL) {
        Arena r = {0};
        return r;
    }
    Arena r = {{(Byte *)p, size}, 0};
    return r;
}

static int os_main(int argc, char **argv, char **envp)
{
    Config conf = {0};
    conf.delim = ':';
    conf.arena = newarena_();

    if (argc) {
        argc--;
        argv++;
    }
    conf.nargs = argc;
    conf.args = allocarray(&conf.arena, SIZEOF(Str), argc);
    for (int i = 0; i < argc; i++) {
        conf.args[i] = fromcstr_(argv[i]);
    }

    conf.fixedpath = S(PKG_CONFIG_LIBDIR);
    conf.sys_incpath = S(PKG_CONFIG_SYSTEM_INCLUDE_PATH);
    conf.sys_libpath = S(PKG_CONFIG_SYSTEM_LIBRARY_PATH);

    for (char **v = envp; *v; v++) {
        Cut c = cut(fromcstr_(*v), '=');
        Str name = c.head;
        Str value = c.tail;

        if (equals(name, S("PKG_CONFIG_PATH"))) {
            conf.envpath = value;
        } else if (equals(name, S("PKG_CONFIG_LIBDIR"))) {
            conf.fixedpath = value;
        } else if (equals(name, S("PKG_CONFIG_TOP_BUILD_DIR"))) {
            conf.top_builddir = value;
        } else if (equals(name, S("PKG_CONFIG_SYSTEM_INCLUDE_PATH"))) {
            conf.sys_incpath = value;
        } else if (equals(name, S("PKG_CONFIG_SYSTEM_LIBRARY_PATH"))) {
            conf.sys_libpath = value;
        }
    }

    appmain(conf);
    return 0;
}

void entrypoint(char **stack)
{
    long   argc = ((long *)stack)[0];
    char **argv = (char **)stack + 1;
    char **envp = argv + argc + 1;
    int status  = os_main((int)argc, argv, envp);
    syscall1(SYS_exit, status);
    __builtin_unreachable();
}
