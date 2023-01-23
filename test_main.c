// Test suite for u-config
// On success prints "all tests pass" (for humans) and exits with a zero
// status (for scripts). Attach a debugger to examine failures in detail.
// This is free and unencumbered software released into the public domain.
#define DEBUG
#include "u-config.c"
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define E (Str){0, 0}
#define SHOULDPASS \
    for (int r = setjmp(context.exit); !r || (r>0 && (TRAP, 0)); r = -1)
#define SHOULDFAIL \
    for (int r = setjmp(context.exit); !r; TRAP)
#define PCHDR "Name:\n" "Version:\n" "Description:\n"

static struct {
    jmp_buf exit;
    Arena arena;
    Str output;
    Str outavail;
    Env filesystem;
} context;

static void os_fail(void)
{
    longjmp(context.exit, 1);
}

static MapFileResult os_mapfile(Arena *a, Str path)
{
    (void)a;
    ASSERT(path.s);
    ASSERT(path.len);
    ASSERT(!path.s[path.len-1]);
    path.len--;  // trim null terminator

    Str *contents = insert(0, &context.filesystem, path);
    if (!contents) {
        MapFileResult r = {.status=MapFile_NOTFOUND};
        return r;
    }
    MapFileResult r = {.contents=*contents, .status=MapFile_OK};
    return r;
}

static void os_write(int fd, Str s)
{
    ASSERT(fd==1 || fd==2);
    if (fd == 1) {
        ASSERT(context.outavail.len >= s.len);
        context.outavail = copy(context.outavail, s);
        context.output.len += s.len;
    }
}

static Config newtest_(char *name)
{
    printf("TEST: %s\n", name);

    context.arena.off = 0;
    context.outavail = newstr(&context.arena, 1<<10);
    context.filesystem = (Env){0};

    Config conf = {0};
    conf.arena = context.arena;
    conf.delim = ':';
    conf.fixedpath = S("/usr/lib/pkgconfig:/usr/share/pkgconfig");
    return conf;
}

static void newfile_(Config *conf, Str path, Str contents)
{
    *insert(&conf->arena, &context.filesystem, path) = contents;
}

static void run(Config conf, ...)
{
    va_list ap;

    va_start(ap, conf);
    for (conf.nargs = 0;; conf.nargs++) {
        Str arg = va_arg(ap, Str);
        if (!arg.s) {
            break;
        }
    }
    va_end(ap);

    conf.args = allocarray(&conf.arena, SIZEOF(Str), conf.nargs);
    va_start(ap, conf);
    for (Size i = 0; i < conf.nargs; i++) {
        conf.args[i] = va_arg(ap, Str);
    }
    va_end(ap);

    context.output = takehead(context.outavail, 0);
    shredfree(&conf.arena);
    appmain(conf);
}

static void test_noargs(void)
{
    // NOTE: this is mainly a sanity check of the test system itself
    Config conf = newtest_("no arguments");
    SHOULDFAIL {
        run(conf, E);
    }
}

static void test_modversion(void)
{
    Config conf = newtest_("--modversion");
    SHOULDPASS {
        newfile_(&conf, S("/usr/lib/pkgconfig/direct.pc"), S(
            "Name:\n"
            "Version: 1.2.3\n"
            "Description:\n"
        ));
        newfile_(&conf, S("/usr/share/pkgconfig/indirect.pc"), S(
            "major = 12\n"
            "minor = 345\n"
            "patch = 6789\n"
            "version = ${major}.${minor}.${patch}\n"
            "Name:\n"
            "Version: ${version}\n"
            "Description:\n"
        ));
        run(conf, S("--modversion"), S("direct"), S("indirect"), E);
        ASSERT(equals(context.output, S("1.2.3\n12.345.6789\n")));
    }
}

static void test_badversion(void)
{
    Config conf = newtest_("version checks");
    newfile_(&conf, S("/usr/lib/pkgconfig/test.pc"), S(
        "Name:\n"
        "Version: 1.2.3\n"
        "Description:\n"
    ));

    SHOULDPASS {
        run(conf, S("--modversion"), S("test = 1.2.3"), E);
    }

    SHOULDFAIL {
        run(conf, S("--modversion"), S("test < 1.2.3"), E);
    }

    SHOULDFAIL {
        run(conf, S("--modversion"), S("test <= 1.2.2"), E);
    }

    SHOULDFAIL {
        run(conf, S("--modversion"), S("test > 1.2.3"), E);
    }

    SHOULDFAIL {
        run(conf, S("--modversion"), S("test >= 1.2.4"), E);
    }
}

static void test_private_transitive(void)
{
    // Scenario: a privately requires b which publicly requires c
    // Expect: --libs should not include c without --static
    Config conf = newtest_("private transitive");
    newfile_(&conf, S("/usr/lib/pkgconfig/a.pc"), S(
        PCHDR
        "Requires: x\n"
        "Requires.private: b\n"
        "Libs: -la\n"
    ));
    newfile_(&conf, S("/usr/lib/pkgconfig/x.pc"), S(
        PCHDR
        "Libs: -lx\n"
    ));
    newfile_(&conf, S("/usr/lib/pkgconfig/b.pc"), S(
        PCHDR
        "Requires: c\n"
        "Libs: -lb\n"
    ));
    newfile_(&conf, S("/usr/lib/pkgconfig/c.pc"), S(
        PCHDR
        "Requires.private: b\n"
        "Libs: -lc\n"
    ));

    SHOULDPASS {
        run(conf, S("--libs"), S("a"), E);
    }
    ASSERT(equals(context.output, S("-la -lx\n")));

    SHOULDPASS {
        run(conf, S("--libs"), S("--static"), S("a"), E);
    }
    ASSERT(equals(context.output, S("-la -lx -lb -lc\n")));
}

static void test_lol(void)
{
    Config conf = newtest_("a billion laughs");
    newfile_(&conf, S("lol.pc"), S(
        "v9=lol\n"
        "v8=${v9}${v9}${v9}${v9}${v9}${v9}${v9}${v9}${v9}${v9}\n"
        "v7=${v8}${v8}${v8}${v8}${v8}${v8}${v8}${v8}${v8}${v8}\n"
        "v6=${v7}${v7}${v7}${v7}${v7}${v7}${v7}${v7}${v7}${v7}\n"
        "v5=${v6}${v6}${v6}${v6}${v6}${v6}${v6}${v6}${v6}${v6}\n"
        "v4=${v5}${v5}${v5}${v5}${v5}${v5}${v5}${v5}${v5}${v5}\n"
        "v3=${v4}${v4}${v4}${v4}${v4}${v4}${v4}${v4}${v4}${v4}\n"
        "v2=${v3}${v3}${v3}${v3}${v3}${v3}${v3}${v3}${v3}${v3}\n"
        "v1=${v2}${v2}${v2}${v2}${v2}${v2}${v2}${v2}${v2}${v2}\n"
        "v0=${v1}${v1}${v1}${v1}${v1}${v1}${v1}${v1}${v1}${v1}\n"
        "Name:\n"
        "Version: ${v0}\n"
        "Description:\n"
    ));
    SHOULDFAIL {
        run(conf, S("--modversion"), S("lol.pc"), E);
    }
}

static Arena newarena_(Size cap)
{
    Arena arena = {0};
    arena.mem.s = malloc(cap);
    arena.mem.len = arena.mem.s ? cap : 0;
    return arena;
}

int main(void)
{
    context.arena = newarena_(1<<21);

    test_noargs();
    test_modversion();
    test_badversion();
    test_private_transitive();
    test_lol();

    free(context.arena.mem.s);  // to satisfy leak checkers
    puts("all tests pass");
    return 0;
}
