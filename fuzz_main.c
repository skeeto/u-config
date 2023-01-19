// afl fuzz test platform layer for u-config
// $ afl-clang-fast -fsanitize=address,undefined fuzz_main.c
// $ afl-fuzz -m32T -i /usr/share/pkgconfig -o fuzzout ./a.out
#define DEBUG
#include "u-config.c"
#include <setjmp.h>
#include <stdlib.h>
#include <unistd.h>  // required by afl

__AFL_FUZZ_INIT();

static jmp_buf ret;
static Str pcfile;

static void os_fail(void)
{
    longjmp(ret, 1);
}

static void os_write(int fd, Str s)
{
    (void)fd;
    (void)s;
}

static MapFileResult os_mapfile(Arena *a, Str path)
{
    (void)a;
    (void)path;
    MapFileResult r = {pcfile, MapFile_OK};
    return r;
}

int main(void)
{
    #ifdef __AFL_HAVE_MANUAL_CONTROL
    __AFL_INIT();
    #endif

    Config conf = {
        .arena = {malloc(1<<16), 1<<16},
        .args = (Str[]){S("--modversion"), S("afl")},
        .nargs = 2,
        .fixedpath = S("/usr/lib/pkgconfig"),
    };

    pcfile.s = __AFL_FUZZ_TESTCASE_BUF;
    while (__AFL_LOOP(10000)) {
        pcfile.len = __AFL_FUZZ_TESTCASE_LEN;
        if (!setjmp(ret)) {
            appmain(conf);
        }
    }
    return 0;
}
