// libc-free platform layer for x86-64 Linux
// This is free and unencumbered software released into the public domain.
#include "src/u-config.c"

enum {
    SYS_close       = 3,
    SYS_exit        = 60,
    SYS_open        = 2,
    SYS_read        = 0,
    SYS_write       = 1,
    SYS_getdents64  = 217,
};

#include "src/linux_noarch.c"

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
