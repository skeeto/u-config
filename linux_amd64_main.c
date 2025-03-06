// libc-free platform layer for x86-64 Linux
// This is free and unencumbered software released into the public domain.
#include "u-config.c"

enum {
    SYS_close   = 3,
    SYS_exit    = 60,
    SYS_mmap    = 9,
    SYS_open    = 2,
    SYS_read    = 0,
    SYS_write   = 1,

    MAP_PRIVATE     = 0x02,
    MAP_ANONYMOUS   = 0x20,
};

#include "linux_noarch.c"

asm (
    "        .globl _start\n"
    "_start: mov   %rsp, %rdi\n"
    "        call  entrypoint\n"
);

static long syscall1(long n, long a)
{
    long r;
    asm volatile (
        "syscall"
        : "=a"(r)
        : "a"(n), "D"(a)
        : "rcx", "r11", "memory"
    );
    return r;
}

static long syscall2(long n, long a, long b)
{
    long r;
    asm volatile (
        "syscall"
        : "=a"(r)
        : "a"(n), "D"(a), "S"(b)
        : "rcx", "r11", "memory"
    );
    return r;
}

static long syscall3(long n, long a, long b, long c)
{
    long r;
    asm volatile (
        "syscall"
        : "=a"(r)
        : "a"(n), "D"(a), "S"(b), "d"(c)
        : "rcx", "r11", "memory"
    );
    return r;
}

static long syscall6(long n, long a, long b, long c, long d, long e, long f)
{
    long r;
    register long r10 asm("r10") = d;
    register long r8  asm("r8")  = e;
    register long r9  asm("r9")  = f;
    asm volatile (
        "syscall"
        : "=a"(r)
        : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory"
    );
    return r;
}
