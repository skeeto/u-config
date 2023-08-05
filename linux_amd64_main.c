// libc-free platform layer for x86-64 Linux
// This is free and unencumbered software released into the public domain.
#include "u-config.c"

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

#define SYS_write 1
#define SYS_open  2
#define SYS_close 3
#define SYS_fstat 5
#define SYS_mmap  9
#define SYS_exit  60

long syscall1(long, long);
long syscall2(long, long, long);
long syscall3(long, long, long, long);
long syscall6(long, long, long, long, long, long, long);

static void os_fail(void)
{
    syscall1(SYS_exit, 1);
    #if __GNUC__
    __builtin_unreachable();
    #endif
}

static void os_write(int fd, Str s)
{
    ASSERT(fd==1 || fd==2);
    while (s.len) {
        long r = syscall3(SYS_write, fd, (long)s.s, s.len);
        if (r < 0) {
            os_fail();
        }
        s = cuthead(s, r);
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

    long stat[18];
    if (syscall2(SYS_fstat, fd, (long)stat) < 0) {
        syscall1(SYS_close, fd);
        MapFileResult r = {.status=MapFile_READERR};
        return r;
    }

    if (stat[6] > Size_MAX) {
        syscall1(SYS_close, fd);
        MapFileResult r = {.status=MapFile_READERR};
        return r;
    }
    Size size = (Size)stat[6];

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

__attribute__((externally_visible))
int os_main(int argc, char **argv, char **envp)
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
