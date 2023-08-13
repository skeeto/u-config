// libc-free platform layer for x86-32 Linux
// This is free and unencumbered software released into the public domain.
#include "u-config.c"

#define SYS_close 6
#define SYS_exit  1
#define SYS_fstat 108
#define SYS_mmap  192  // actually mmap2
#define SYS_open  5
#define SYS_write 4

struct stat {
    long pad1[5];
    long st_size;
    long pad2[16];
};

#include "linux_noarch.c"

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

static long syscall2(long n, long a, long b)
{
    long r;
    asm volatile (
        "int $0x80"
        : "=a"(r)
        : "a"(n), "b"(a), "c"(b)
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

static long syscall6(long n, long a, long b, long c, long d, long e, long f)
{
    long r;
    asm volatile (
        "push  %7\n"
        "push  %%ebp\n"
        "mov   4(%%esp), %%ebp\n"
        "int   $0x80\n"
        "pop   %%ebp\n"
        "add   $4, %%esp\n"
        : "=a"(r)
        : "a"(n), "b"(a), "c"(b), "d"(c), "S"(d), "D"(e), "m"(f)
        : "memory"
    );
    return r;
}
