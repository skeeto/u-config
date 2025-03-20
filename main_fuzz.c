// afl fuzz test platform layer for u-config
// $ afl-gcc-fast -fsanitize=undefined -fsanitize-trap main_fuzz.c
// $ afl-fuzz -i /usr/share/pkgconfig -o fuzzout ./a.out
#define FUZZTEST
#include "src/u-config.c"
#include <setjmp.h>
#include <stdlib.h>
#include <unistd.h>  // required by afl

__AFL_FUZZ_INIT();

static jmp_buf ret;
static s8 pcfile;

static void os_fail(void)
{
    longjmp(ret, 1);
}

static void os_write(int fd, s8 s)
{
    (void)fd;
    (void)s;
}

static filemap os_mapfile(arena *perm, s8 path)
{
    (void)perm;
    (void)path;
    filemap r = {0};
    r.data = pcfile;
    r.status = filemap_OK;
    return r;
}

int main(void)
{
    __AFL_INIT();
    s8 args[] = {S("--static"), S("--cflags"), S("--libs"), S("afl")};

    iz cap = 1<<16;
    arena perm = {0};
    perm.beg = malloc(cap);
    perm.end = perm.beg + cap;

    pcfile.s = __AFL_FUZZ_TESTCASE_BUF;
    while (__AFL_LOOP(10000)) {
        pcfile.len = __AFL_FUZZ_TESTCASE_LEN;
        config conf = {0};
        conf.perm = perm;
        conf.args = args;
        conf.nargs = countof(args);
        conf.fixedpath = S("/usr/lib/pkgconfig");
        if (!setjmp(ret)) {
            uconfig(&conf);
        }
    }
}
