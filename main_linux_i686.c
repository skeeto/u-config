// libc-free platform layer for x86-32 Linux
// This is free and unencumbered software released into the public domain.
#include "src/u-config.c"

enum {
    SYS_close       = 6,
    SYS_exit        = 1,
    SYS_openat      = 295,
    SYS_read        = 3,
    SYS_write       = 4,
    SYS_getdents64  = 220,

    O_DIRECTORY     = 0x10000,
};

#include "src/linux_noarch.c"

asm (
    "        .globl _start\n"
    "_start: mov   %esp, %eax\n"
    "        sub   $12, %esp\n"
    "        push  %eax\n"
    "        call  entrypoint\n"
);

static long syscall1(long n, long a)
{
    long r;
    asm volatile (
        "int $0x80"
        : "=a"(r)
        : "a"(n), "b"(a)
        : "memory"
    );
    return r;
}

static long syscall3(long n, long a, long b, long c)
{
    long r;
    asm volatile (
        "int $0x80"
        : "=a"(r)
        : "a"(n), "b"(a), "c"(b), "d"(c)
        : "memory"
    );
    return r;
}
