// afl fuzz test platform layer for u-config
// $ afl-gcc-fast -fsanitize=undefined -fsanitize-trap main_fuzz.c
// $ afl-fuzz -i /usr/share/pkgconfig -o fuzzout ./a.out
#define FUZZTEST
#include "src/u-config.c"
#include <setjmp.h>
#include <stdlib.h>
#include <unistd.h>  // required by afl

__AFL_FUZZ_INIT();

struct os {
    jmp_buf ret;
    s8      pcfile;
};

static void os_fail(os *ctx)
{
    longjmp(ctx->ret, 1);
}

static void os_write(os *ctx, i32 fd, s8 s)
{
    (void)ctx;
    (void)fd;
    (void)s;
}

static filemap os_mapfile(os *ctx, arena *perm, s8 path)
{
    (void)perm;
    (void)path;
    filemap r = {0};
    r.data = ctx->pcfile;
    r.status = filemap_OK;
    return r;
}

int main(void)
{
    __AFL_INIT();
    u8 *args[] = {
        S("--static").s, S("--cflags").s, S("--libs").s, S("afl").s,
    };

    iz cap = 1<<16;
    arena perm = {0};
    perm.beg = malloc(cap);
    perm.end = perm.beg + cap;

    os ctx = {0};
    ctx.pcfile.s = __AFL_FUZZ_TESTCASE_BUF;
    while (__AFL_LOOP(10000)) {
        ctx.pcfile.len = __AFL_FUZZ_TESTCASE_LEN;
        config conf = {0};
        conf.perm = perm;
        conf.perm.ctx = &ctx;
        conf.args = args;
        conf.nargs = countof(args);
        conf.fixedpath = S("/usr/lib/pkgconfig");
        if (!setjmp(ctx.ret)) {
            uconfig(&conf);
        }
    }
}
