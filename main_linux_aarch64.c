// libc-free platform layer for aarch64 Linux
// This is free and unencumbered software released into the public domain.
#include "src/u-config.c"

enum {
    SYS_close       = 57,
    SYS_exit        = 93,
    SYS_openat      = 56,
    SYS_read        = 63,
    SYS_write       = 64,
    SYS_getdents64  = 61,

    O_DIRECTORY     = 0x4000,
};

#include "src/linux_noarch.c"

asm (
    "        .globl _start\n"
    "_start: mov x0, sp\n"
    "        bl  entrypoint\n"
);

static long syscall1(long n, long a)
{
    register long x8 asm("x8") = n;
    register long x0 asm("x0") = a;
    asm volatile (
        "svc 0"
        : "=r"(x0)
        : "0"(x0), "r"(x8)
        : "memory", "cc"
    );
    return x0;
}

static long syscall3(long n, long a, long b, long c)
{
    register long x8 asm("x8") = n;
    register long x0 asm("x0") = a;
    register long x1 asm("x1") = b;
    register long x2 asm("x2") = c;
    asm volatile (
        "svc 0"
        : "=r"(x0)
        : "0"(x0), "r"(x8), "r"(x1), "r"(x2)
        : "memory", "cc"
    );
    return x0;
}
