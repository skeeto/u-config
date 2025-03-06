// libc-free platform layer for amd64 OpenBSD
// $ cc -static -nostartfiles -no-pie -o pkg-config openbsd_main.c
// This is free and unencumbered software released into the public domain.
#include "u-config.c"

enum {
    SYS_exit    = 1,
    SYS_read    = 3,
    SYS_write   = 4,
    SYS_open    = 5,
    SYS_close   = 6,
    SYS_mmap    = 49,

    MAP_PRIVATE     = 0x0002,
    MAP_ANONYMOUS   = 0x1000,
};

#include "linux_noarch.c"

static long syscall1(long n, long a)
{
    return syscall6(n, a, 0, 0, 0, 0, 0);
}

static long syscall2(long n, long a, long b)
{
    return syscall6(n, a, b, 0, 0, 0, 0);
}

static long syscall3(long n, long a, long b, long c)
{
    return syscall6(n, a, b, c, 0, 0, 0);
}

__attribute((noinline))
static long syscall6(long n, long a, long b, long c, long d, long e, long f)
{
    char err;
    register long r10 asm("r10") = d;
    register long r8  asm("r8")  = e;
    register long r9  asm("r9")  = f;
    asm volatile (
        "_syscall6: syscall\n"
        : "+a"(n), "+d"(c), "=@ccc"(err)
        : "D"(a), "S"(b), "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory"
    );
    return err ? -n : n;
}

asm (
    "        .globl __start\n"
    "__start:mov   %rsp, %rdi\n"
    "        call  entrypoint\n"

    ".pushsection .note.openbsd.ident, \"a\"\n"
    ".long  8, 4, 1, 0x6e65704f, 0x00445342, 0\n"
    ".popsection\n"

    ".pushsection .openbsd.syscalls\n"
    ".long  _syscall6, 1\n"
    ".long  _syscall6, 3\n"
    ".long  _syscall6, 4\n"
    ".long  _syscall6, 5\n"
    ".long  _syscall6, 6\n"
    ".long  _syscall6, 49\n"
    ".popsection\n"
);
