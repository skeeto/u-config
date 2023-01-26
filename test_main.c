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

#define E S("")
#define SHOULDPASS \
    for (int r = setjmp(context.exit); !r || (r>0 && (TRAP, 0)); r = -1)
#define SHOULDFAIL \
    for (int r = setjmp(context.exit); !r; TRAP)
#define PCHDR "Name:\n" "Version:\n" "Description:\n"
#define EXPECT(w) \
    if (!equals(context.output, S(w))) { \
        printf("EXPECT: %s", w); \
        printf("OUTPUT: %.*s", (int)context.output.len, context.output.s); \
        fflush(stdout); \
        TRAP; \
    }

static struct {
    jmp_buf exit;
    Arena arena;
    Str outbuf;
    Str output;
    Str outavail;
    Env filesystem;
    Bool active;
} context;

static void os_fail(void)
{
    ASSERT(context.active);
    context.active = 0;
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
        MapFileResult r = {{0, 0}, MapFile_NOTFOUND};
        return r;
    }
    MapFileResult r = {*contents, MapFile_OK};
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

static Config newtest_(Str name)
{
    printf("TEST: %.*s\n", (int)name.len, (char *)name.s);

    Env empty = {0};
    context.filesystem = empty;
    context.arena.off = 0;
    context.outbuf = newstr(&context.arena, 1<<10);

    Config conf = {0};
    conf.arena = context.arena;
    conf.delim = ':';
    conf.sys_incpath = S("/usr/include");
    conf.sys_libpath = S("/lib:/usr/lib");
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
        if (!arg.len) {
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

    context.output = takehead(context.outbuf, 0);
    context.outavail = context.outbuf;
    shredfree(&conf.arena);
    context.active = 1;
    appmain(conf);
    ASSERT(context.active);
    context.active = 0;
}

static void test_noargs(void)
{
    // NOTE: this is mainly a sanity check of the test system itself
    Config conf = newtest_(S("no arguments"));
    SHOULDFAIL {
        run(conf, E);
    }
}

static void test_modversion(void)
{
    Config conf = newtest_(S("--modversion"));
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
        EXPECT("1.2.3\n12.345.6789\n");
    }
}

static void test_badversion(void)
{
    Config conf = newtest_(S("version checks"));
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

static void test_maximum_traverse_depth(void)
{
    Config conf = newtest_(S("--maximum-traverse-depth"));
    newfile_(&conf, S("/usr/lib/pkgconfig/a.pc"), S(
        PCHDR
        "Requires: b\n"
        "Cflags: -Da\n"
    ));
    newfile_(&conf, S("/usr/lib/pkgconfig/b.pc"), S(
        PCHDR
        "Requires: c\n"
        "Cflags: -Db\n"
    ));
    newfile_(&conf, S("/usr/lib/pkgconfig/c.pc"), S(
        PCHDR
        "Cflags: -Dc\n"
    ));

    SHOULDPASS {
        run(conf, S("--maximum-traverse-depth=1"), S("--cflags"), S("a"), E);
    }
    EXPECT("-Da\n");

    SHOULDPASS {
        run(conf, S("--maximum-traverse-depth=2"), S("--cflags"), S("a"), E);
    }
    EXPECT("-Da -Db\n");

    SHOULDPASS {
        run(conf, S("--maximum-traverse-depth=3"), S("--cflags"), S("a"), E);
    }
    EXPECT("-Da -Db -Dc\n");
}

static void test_private_transitive(void)
{
    // Scenario: a privately requires b which publicly requires c
    // Expect: --libs should not include c without --static
    Config conf = newtest_(S("private transitive"));
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
    EXPECT("-la -lx\n");

    SHOULDPASS {
        run(conf, S("--libs"), S("--static"), S("a"), E);
    }
    EXPECT("-la -lx -lb -lc\n");
}

static void test_revealed_transitive(void)
{
    // Scenario: a privately requires b, which requires x
    // Expect: "--libs a" lists only a, "--libs a b" reveals x
    //
    // The trouble is that x is initially loaded private. However, when
    // laoding b it should become public, and so must be revisited in
    // traversal and marked as such.
    Config conf = newtest_(S("revealed transitive"));
    newfile_(&conf, S("/usr/lib/pkgconfig/a.pc"), S(
        PCHDR
        "Requires.private: b\n"
        "Libs: -la\n"
    ));
    newfile_(&conf, S("/usr/lib/pkgconfig/b.pc"), S(
        PCHDR
        "Requires: x\n"
    ));
    newfile_(&conf, S("/usr/lib/pkgconfig/x.pc"), S(
        PCHDR
        "Libs: -lx\n"
    ));

    SHOULDPASS {
        run(conf, S("--libs"), S("a"), E);
    }
    EXPECT("-la\n");

    SHOULDPASS {
        run(conf, S("--libs"), S("a"), S("b"), E);
    }
    EXPECT("-la -lx\n");
}

static void test_syspaths(void)
{
    Config conf = newtest_(S("exclude syspaths"));
    newfile_(&conf, S("/usr/lib/pkgconfig/example.pc"), S(
        PCHDR
        "prefix=/usr\n"
        "Cflags: -DEXAMPLE -I${prefix}/include\n"
        "Libs: -L${prefix}/lib -lexample\n"
    ));
    SHOULDPASS {
        run(conf, S("--cflags"), S("--libs"), S("example"), E);
    }
    EXPECT("-DEXAMPLE -lexample\n");
}

static void test_windows(void)
{
    // Tests the ';' delimiter, that the prefix is overridden, and that
    // prefixes containing spaces are properly quoted. The fixed path
    // would be Win32 platform's fixed path if the binary was located in
    // "$HOME/bin".
    Config conf = newtest_(S("windows"));
    conf.fixedpath = S(
        "C:/Documents and Settings/John Falstaff/lib/pkgconfig;"
        "C:/Documents and Settings/John Falstaff/share/pkgconfig"
    );
    conf.envpath = S(
        "C:/Program Files/Example/lib/pkgconfig;"
        "C:/Program Files/SDL2/x86_64-w64-mingw32/lib/pkgconfig"
    );
    conf.sys_incpath = S(
        "C:/w64devkit/x86_64-w64-mingw32/include;"
        "C:/Documents and Settings/John Falstaff/include"
    );
    conf.sys_libpath = S(
        "C:/w64devkit/x86_64-w64-mingw32/lib;"
        "C:/Documents and Settings/John Falstaff/lib"
    );
    conf.define_prefix = 1;
    conf.delim = ';';
    newfile_(&conf, S(
        "C:/Documents and Settings/John Falstaff/lib/pkgconfig/example.pc"
    ), S(
        PCHDR
        "prefix=/usr\n"
        "libdir=${prefix}/lib\n"
        "includedir=${prefix}/include\n"
        "Libs: -L${libdir} -lexample\n"
        "Cflags: -I${includedir}\n"
    ));
    newfile_(&conf, S(
        "C:/Program Files/SDL2/x86_64-w64-mingw32/lib/pkgconfig/sdl2.pc"
    ), S(
        "prefix=/opt/local/x86_64-w64-mingw32\n"
        "exec_prefix=${prefix}\n"
        "libdir=${exec_prefix}/lib\n"
        "includedir=${prefix}/include\n"
        "Name: sdl2\n"
        "Description: Simple DirectMedia Layer\n"
        "Version: 2.26.2\n"
        "Libs: -L${libdir} -lmingw32 -lSDL2main -lSDL2 -mwindows\n"
        "Cflags: -I${includedir} -I${includedir}/SDL2  -Dmain=SDL_main\n"
    ));

    SHOULDPASS {
        run(conf, S("--cflags"), S("--libs"), S("example"), E);
    }
    EXPECT("-lexample\n");

    SHOULDPASS {
        run(conf, S("--cflags"), S("--libs"), S("sdl2"), E);
    }
    EXPECT(
        "-IC:/Program\\ Files/SDL2/x86_64-w64-mingw32/include "
        "-IC:/Program\\ Files/SDL2/x86_64-w64-mingw32/include/SDL2 "
        "-Dmain=SDL_main "
        "-LC:/Program\\ Files/SDL2/x86_64-w64-mingw32/lib "
        "-lmingw32 -lSDL2main -lSDL2 -mwindows\n"
    );
}

static void outlong_(Out *out, long x)
{
    Byte buf[32];
    Byte *e = buf + SIZEOF(buf);
    Byte *p = e;
    do {
        *--p = (Byte)(x%10 + '0');
    } while (x /= 10);
    outstr(out, fromptrs(p, e));
}

static void test_manyvars(void)
{
    // Stresses the treap-backed package environment
    Config conf = newtest_(S("many variables"));
    newfile_(&conf, S("manyvars.pc"), S(""));  // allocate empty file
    long nvars = 10000;

    for (long i = 0; i < nvars; i += 197) {
        Config temp = conf;
        Byte prefix = 'a' + (Byte)(i%26);

        // Write a fresh .pc file into the virtual "manyvars.pc" with a
        // rotated variable order and prefix to perturb the package Env.
        Out pc = newmembuf(&temp.arena);
        outstr(&pc, S(PCHDR));
        for (long v = 0; v < nvars; v++) {
            long vnum = (v + i) % nvars;
            outbyte(&pc, prefix);
            outlong_(&pc, vnum);
            outbyte(&pc, '=');
            outbyte(&pc, 'A' + (Byte)(vnum%26));
            outbyte(&pc, '\n');
        }
        newfile_(&temp, S("manyvars.pc"), finalize(&pc));  // overwrite

        // Probe a variable to test the environment
        Out mem = newmembuf(&temp.arena);
        outbyte(&mem, prefix);
        outlong_(&mem, i);
        Str var = finalize(&mem);
        SHOULDPASS {
            run(temp, S("manyvars.pc"), S("--variable"), var, E);
        }
        Byte expect[] = {'A' + (Byte)(i%26), '\n', 0};
        EXPECT(expect);
    }
}

static void test_lol(void)
{
    Config conf = newtest_(S("a billion laughs"));
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
    test_maximum_traverse_depth();
    test_private_transitive();
    test_revealed_transitive();
    test_syspaths();
    test_windows();
    test_manyvars();
    test_lol();

    free(context.arena.mem.s);  // to satisfy leak checkers
    puts("all tests pass");
    return 0;
}
