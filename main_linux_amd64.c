// libc-free platform layer for x86-64 Linux
// This is free and unencumbered software released into the public domain.
#include "src/u-config.c"

enum {
    SYS_close       = 3,
    SYS_exit        = 60,
    SYS_openat      = 257,
    SYS_read        = 0,
    SYS_write       = 1,
    SYS_getdents64  = 217,

    O_DIRECTORY     = 0x10000,
};

#include "src/linux_noarch.c"

asm (
    "        .globl _start\n"
    "_start: mov   %rsp, %rdi\n"
    "        call  entrypoint\n"
);

static iz syscall1(iz n, iz a)
{
    iz r;
    asm volatile (
        "syscall"
        : "=a"(r)
        : "a"(n), "D"(a)
        : "rcx", "r11", "memory"
    );
    return r;
}

static iz syscall3(iz n, iz a, iz b, iz c)
{
    iz r;
    asm volatile (
        "syscall"
        : "=a"(r)
        : "a"(n), "D"(a), "S"(b), "d"(c)
        : "rcx", "r11", "memory"
    );
    return r;
}
