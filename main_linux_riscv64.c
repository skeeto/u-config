// libc-free platform layer for riscv64 Linux
// This is free and unencumbered software released into the public domain.
#include "src/u-config.c"

enum {
    SYS_close       = 57,
    SYS_exit        = 93,
    SYS_openat      = 56,
    SYS_read        = 63,
    SYS_write       = 64,
    SYS_getdents64  = 61,

    O_DIRECTORY     = 0x10000,
};

#include "src/linux_noarch.c"

asm (
    "        .globl _start\n"
    "_start: mv   a0, sp\n"
    "        call entrypoint\n"
);

static long syscall1(long n, long a)
{
    register long a7 asm("a7") = n;
    register long a0 asm("a0") = a;
    asm volatile (
        "ecall"
        : "=r"(a0)
        : "0"(a0), "r"(a7)
        : "memory"
    );
    return a0;
}

static long syscall3(long n, long a, long b, long c)
{
    register long a7 asm("a7") = n;
    register long a0 asm("a0") = a;
    register long a1 asm("a1") = b;
    register long a2 asm("a2") = c;
    asm volatile (
        "ecall"
        : "=r"(a0)
        : "0"(a0), "r"(a7), "r"(a1), "r"(a2)
        : "memory"
    );
    return a0;
}
