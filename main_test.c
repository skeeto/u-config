// Test suite for u-config
// On success prints "all tests pass" (for humans) and exits with a zero
// status (for scripts). Attach a debugger to examine failures in detail.
// This is free and unencumbered software released into the public domain.
#ifndef __GNUC__
#  define __builtin_unreachable()  *(volatile int *)0 = 0
#  define __builtin_trap()         *(volatile int *)0 = 0
#  define __attribute(x)
#endif
#include "src/u-config.c"

#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define E S("")
#define SHOULDPASS \
    for (i32 r = setjmp(conf.perm.ctx->exit); \
         !r || (r>0 && (__builtin_trap(), 0)); \
         r = -1)
#define SHOULDFAIL \
    for (i32 r = setjmp(conf.perm.ctx->exit); !r; __builtin_trap())
#define PCHDR "Name:\n" "Version:\n" "Description:\n"
#define EXPECT(w) \
    if (!s8equals(conf.perm.ctx->output, S(w))) { \
        printf("EXPECT: %s", w); \
        printf("OUTPUT: %.*s", \
               (int)conf.perm.ctx->output.len, conf.perm.ctx->output.s); \
        fflush(stdout); \
        __builtin_trap(); \
    }
#define MATCH(w) \
    if (s8find_(conf.perm.ctx->output, S(w)) == conf.perm.ctx->output.len) { \
        printf("MATCH:  %s\n", w); \
        printf("OUTPUT: %.*s", \
               (int)conf.perm.ctx->output.len, conf.perm.ctx->output.s); \
        fflush(stdout); \
        __builtin_trap(); \
    }

struct os {
    jmp_buf exit;
    arena   perm;
    s8      outbuf;
    s8      output;
    s8      outavail;
    env    *filesystem;
    b32     active;
};

static void os_fail(os *ctx)
{
    assert(ctx->active);
    ctx->active = 0;
    longjmp(ctx->exit, 1);
}

static filemap os_mapfile(os *ctx, arena *perm, s8 path)
{
    (void)perm;
    assert(path.s);
    assert(path.len);
    assert(!path.s[path.len-1]);
    path.len--;  // trim null terminator

    filemap r = {0};

    s8 *data = insert(&ctx->filesystem, path, 0);
    if (!data) {
        r.status = filemap_NOTFOUND;
        return r;
    }
    r.data = *data;
    r.status = filemap_OK;
    return r;
}

static void os_write(os *ctx, i32 fd, s8 s)
{
    assert(fd==1 || fd==2);
    assert(ctx->outavail.len >= s.len);
    ctx->outavail = s8copy(ctx->outavail, s);
    ctx->output.len += s.len;
}

// Return needle offset, or the length of haystack on no match.
static iz s8find_(s8 haystack, s8 needle)
{
    u32 match = 0;
    for (iz i = 0; i < needle.len; i++) {
        match = match*257u + needle.s[i];
    }

    u32 f = 1;
    u32 x = 257;
    for (iz n = needle.len-1; n>0; n /= 2) {
        f *= n&1 ? x : 1;
        x *= x;
    }

    iz i = 0;
    u32 hash = 0;
    for (; i<needle.len-1 && i<haystack.len; i++) {
        hash = hash*257u + haystack.s[i];
    }
    for (; i < haystack.len; i++) {
        hash = hash*257u + haystack.s[i];
        iz beg = i - needle.len + 1;
        s8 tail = cuthead(haystack, beg);
        if (hash==match && startswith(tail, needle)) {
            return beg;
        }
        hash -= f * haystack.s[beg];
    }
    return haystack.len;
}

static config newtest_(arena a, s8 name)
{
    printf("TEST: %.*s\n", (int)name.len, (char *)name.s);

    os *ctx = new(&a, os, 1);
    ctx->outbuf = news8(&a, 1<<10);

    config conf = {0};
    conf.delim = ':';
    conf.sys_incpath = S("/usr/include");
    conf.sys_libpath = S("/lib:/usr/lib");
    conf.fixedpath = S("/usr/lib/pkgconfig:/usr/share/pkgconfig");
    conf.perm = a;
    conf.perm.ctx = ctx;
    return conf;
}

static void newfile_(config *conf, s8 path, s8 contents)
{
    os *ctx = conf->perm.ctx;
    *insert(&ctx->filesystem, path, &conf->perm) = contents;
}

static void run(config conf, ...)
{
    va_list ap;

    va_start(ap, conf);
    for (conf.nargs = 0;; conf.nargs++) {
        s8 arg = va_arg(ap, s8);
        if (!arg.len) {
            break;
        }
    }
    va_end(ap);

    conf.args = new(&conf.perm, u8 *, conf.nargs);
    va_start(ap, conf);
    for (iz i = 0; i < conf.nargs; i++) {
        conf.args[i] = va_arg(ap, s8).s;
    }
    va_end(ap);

    os *ctx = conf.perm.ctx;
    ctx->output = takehead(ctx->outbuf, 0);
    ctx->outavail = ctx->outbuf;
    fillbytes(conf.perm.beg, 0x55, conf.perm.end-conf.perm.beg);
    ctx->active = 1;
    uconfig(&conf);
    assert(ctx->active);
    ctx->active = 0;
}

static void test_noargs(arena a)
{
    // NOTE: this is mainly a sanity check of the test system itself
    config conf = newtest_(a, S("no arguments"));
    SHOULDFAIL {
        run(conf, E);
    }
}

static void test_dashdash(arena a)
{
    config conf = newtest_(a, S("handle -- argument"));
    newfile_(&conf, S("/usr/lib/pkgconfig/--foo.pc"), S(
        PCHDR
        "Cflags: -Dfoo\n"
    ));
    newfile_(&conf, S("/usr/lib/pkgconfig/--.pc"), S(
        PCHDR
        "Cflags: -Ddashdash\n"
    ));
    SHOULDPASS {
        run(conf, S("--cflags"), S("--"), S("--foo"), S("--"), E);
    }
    EXPECT("-Dfoo -Ddashdash\n");
}

static void test_modversion(arena a)
{
    config conf = newtest_(a, S("--modversion"));
    newfile_(&conf, S("/usr/lib/pkgconfig/direct.pc"), S(
        "Name:\n"
        "Version: 1.2.3\n"
        "Description:\n"
        "Requires: req"
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
    newfile_(&conf, S("/usr/share/pkgconfig/req.pc"), S(
        "version = 420.69.1337\n"
        "Name:\n"
        "Version: ${version}\n"
        "Description:\n"
    ));
    SHOULDPASS {
        run(conf, S("--modversion"), S("direct"), S("indirect"), E);
        EXPECT("1.2.3\n12.345.6789\n");
    }
    SHOULDPASS {
        // One package is listed twice, first by its name and discovered
        // by searching the path, and second by directly by its path. It
        // must be recognized internally as the same package and so only
        // print one version.
        run(conf, S("--modversion"), S("direct"),
                S("/usr/lib/pkgconfig/direct.pc"), E);
        EXPECT("1.2.3\n");
    }
}

static void test_versioncheck(arena a)
{
    config conf = newtest_(a, S("version checks"));
    newfile_(&conf, S("/usr/lib/pkgconfig/test.pc"), S(
        "Name:\n"
        "Version: 1.2.3\n"
        "Description:\n"
    ));

    SHOULDPASS {
        run(conf, S("--modversion"), S("test = 1.2.3"), E);
    }
    SHOULDPASS {
        run(conf, S("--modversion"), S("test "), S("= 1.2.3"), E);
    }
    SHOULDPASS {
        run(conf, S("--modversion"), S("test "), S("="), S(" 1.2.3"), E);
    }
    SHOULDPASS {
        run(conf, S("--modversion"), S("test ="), S("1.2.3"), E);
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

    SHOULDFAIL {
        run(conf, S("--modversion"), S("test ="), E);
    }
    SHOULDFAIL {
        run(conf, S("--modversion"), S("test"), S("="), E);
    }
}

static void test_versionorder(arena a)
{
    // Scenario: liba depends on libc and libb
    // Expect: modversion order is unaffected
    config conf = newtest_(a, S("library version ordering"));
    newfile_(&conf, S("/usr/lib/pkgconfig/a.pc"), S(
        "Name: \n"
        "Description: \n"
        "Version: 1\n"
        "Requires: c b\n"
    ));
    newfile_(&conf, S("/usr/lib/pkgconfig/b.pc"), S(
        "Name: \n"
        "Description: \n"
        "Version: 2\n"
    ));
    newfile_(&conf, S("/usr/lib/pkgconfig/c.pc"), S(
        "Name: \n"
        "Description: \n"
        "Version: 3\n"
    ));
    SHOULDPASS {
        run(conf, S("--modversion"), S("a"), S("b"), S("c"), E);
    }
    EXPECT("1\n2\n3\n");
}

static void test_overrides(arena a)
{
    config conf = newtest_(a, S("--{atleast,exact,max}-version"));
    newfile_(&conf, S("/usr/lib/pkgconfig/t.pc"), S(
        "Name:\n"
        "Version: 1\n"
        "Description:\n"
    ));

    SHOULDPASS {
        run(conf, S("--atleast-version=0"), S("t > 1"), E);
    }
    SHOULDPASS {
        run(conf, S("--exact-version=1"), S("t > 1"), E);
    }
    SHOULDPASS {
        run(conf, S("--max-version=2"), S("t > 1"), E);
    }

    SHOULDFAIL {
        run(conf, S("--atleast-version=2"), S("t = 1"), E);
    }
    SHOULDFAIL {
        run(conf, S("--exact-version=2"), S("t = 1"), E);
    }
    SHOULDFAIL {
        run(conf, S("--max-version=0"), S("t = 1"), E);
    }

    SHOULDFAIL {
        run(conf, S("--atleast-version=2"), S("t"), E);
    }
    SHOULDFAIL {
        run(conf, S("--exact-version=2"), S("t"), E);
    }
    SHOULDFAIL {
        run(conf, S("--max-version=0"), S("t"), E);
    }
}

static void test_maximum_traverse_depth(arena a)
{
    config conf = newtest_(a, S("--maximum-traverse-depth"));
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

static void test_private_cflags(arena a)
{
    // Scenario: a has private cflags
    // Expect: --cflags should not output it unless --static is also given
    config conf = newtest_(a, S("private cflags"));
    newfile_(&conf, S("/usr/lib/pkgconfig/a.pc"), S(
        PCHDR
        "Cflags: -DA_PUB\n"
        "Cflags.private: -DA_PRIV\n"
        "Libs: -la\n"
    ));
    // Scenario: b has private cflags and so does its dependencies
    // Expect: only output private flags if --static is given
    newfile_(&conf, S("/usr/lib/pkgconfig/b.pc"), S(
        PCHDR
        "Cflags: -DB_PUB\n"
        "Cflags.private: -DB_PRIV\n"
        "Libs: -lb\n"
        "Requires: c\n"
        "Requires.private: d\n"
    ));
    newfile_(&conf, S("/usr/lib/pkgconfig/c.pc"), S(
        PCHDR
        "Cflags: -DC_PUB\n"
        "Cflags.private: -DC_PRIV\n"
        "Libs: -lc\n"
    ));
    newfile_(&conf, S("/usr/lib/pkgconfig/d.pc"), S(
        PCHDR
        "Cflags: -DD_PUB\n"
        "Cflags.private: -DD_PRIV\n"
        "Libs: -ld\n"
    ));

    SHOULDPASS {
        run(conf, S("--cflags"), S("a"), E);
    }
    EXPECT("-DA_PUB\n");

    SHOULDPASS {
        run(conf, S("--static"), S("--cflags"), S("a"), E);
    }
    EXPECT("-DA_PUB -DA_PRIV\n");

    SHOULDPASS {
        run(conf, S("--libs"), S("--static"), S("a"), E);
    }
    EXPECT("-la\n");

    SHOULDPASS {
        run(conf, S("--cflags"), S("b"), E);
    }
    EXPECT("-DB_PUB -DC_PUB -DD_PUB\n");

    SHOULDPASS {
        run(conf, S("--cflags"), S("--static"), S("b"), E);
    }
    EXPECT("-DB_PUB -DB_PRIV -DC_PUB -DC_PRIV -DD_PUB -DD_PRIV\n");
}

static void test_private_transitive(arena a)
{
    // Scenario: a privately requires b which publicly requires c
    // Expect: --libs should not include c without --static
    config conf = newtest_(a, S("private transitive"));
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

static void test_revealed_transitive(arena a)
{
    // Scenario: a privately requires b, which requires x
    // Expect: "--libs a" lists only a, "--libs a b" reveals x
    //
    // The trouble is that x is initially loaded private. However, when
    // loading b it should become public, and so must be revisited in
    // traversal and marked as such.
    config conf = newtest_(a, S("revealed transitive"));
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

static void test_syspaths(arena a)
{
    config conf = newtest_(a, S("exclude syspaths"));
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
    SHOULDPASS {
        run(conf, S("--cflags"), S("--libs"), S("example"),
                S("--keep-system-cflags"), E);
    }
    EXPECT("-DEXAMPLE -I/usr/include -lexample\n");
    SHOULDPASS {
        config copy = conf;
        copy.print_sysinc = S("");
        run(copy, S("--cflags"), S("--libs"), S("example"), E);
    }
    EXPECT("-DEXAMPLE -I/usr/include -lexample\n");
    SHOULDPASS {
        run(conf, S("--cflags"), S("--libs"), S("example"),
                S("--keep-system-libs"), E);
    }
    EXPECT("-DEXAMPLE -L/usr/lib -lexample\n");
    SHOULDPASS {
        config copy = conf;
        copy.print_syslib = S("");
        run(copy, S("--cflags"), S("--libs"), S("example"), E);
    }
    EXPECT("-DEXAMPLE -L/usr/lib -lexample\n");
}

static void test_libsorder(arena a)
{
    // Scenario: two packages link a common library
    // Expect: the common library is listed after both, other flags
    //   maintain their first-seen position and de-duplicate the rest
    config conf = newtest_(a, S("library ordering"));
    newfile_(&conf, S("/usr/lib/pkgconfig/a.pc"), S(
        PCHDR
        "Cflags: -DA -DGL\n"
        "Libs: -L/opt/lib -pthread -mwindows -la -lopengl32\n"
    ));
    newfile_(&conf, S("/usr/lib/pkgconfig/b.pc"), S(
        PCHDR
        "Cflags: -DB -DGL\n"
        "Libs: -L/opt/lib -pthread -mwindows -lb -lopengl32\n"
    ));
    SHOULDPASS {
        run(conf, S("--cflags"), S("--libs"), S("a b"), E);
    }
    EXPECT("-DA -DGL -DB -L/opt/lib -pthread -mwindows -la -lb -lopengl32\n");
}

static void test_staticorder(arena a)
{
    // Scenario: liba depends on libc and libb, libb depends on libc
    // Expect: libc is listed last
    config conf = newtest_(a, S("static library ordering"));
    newfile_(&conf, S("/usr/lib/pkgconfig/a.pc"), S(
        PCHDR
        "Requires.private: c b\n"
        "Libs: -la\n"
    ));
    newfile_(&conf, S("/usr/lib/pkgconfig/b.pc"), S(
        PCHDR
        "Requires.private: c\n"
        "Libs: -lb\n"
    ));
    newfile_(&conf, S("/usr/lib/pkgconfig/c.pc"), S(
        PCHDR
        "Libs: -lc\n"
    ));
    SHOULDPASS {
        run(conf, S("--static"), S("--libs"), S("a"), E);
    }
    EXPECT("-la -lb -lc\n");
}

static void test_windows(arena a)
{
    // Tests the ';' delimiter, that the prefix is overridden, and that
    // prefixes containing spaces are properly quoted. The fixed path
    // would be Win32 platform's fixed path if the binary was located in
    // "$HOME/bin".
    config conf = newtest_(a, S("windows"));
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
        "Cflags: -I\"${includedir}\"\n"
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

static void test_parens(arena a)
{
    // Test if that paths allow parenthesis, but also that parenthesis
    // otherwise still work as meta characters.
    config conf = newtest_(a, S("parens"));
    conf.fixedpath = S(
        "C:/Program Files (x86)/Contoso/lib/pkgconfig"
    );
    conf.define_prefix = 1;
    conf.delim = ';';
    newfile_(&conf, S(
        "C:/Program Files (x86)/Contoso/lib/pkgconfig/example.pc"
    ), S(
        PCHDR
        "prefix=/usr/local\n"
        "Cflags: -I${pc_top_builddir}/include -I${prefix}/include\n"
        "Libs: -L\"${pc_top_builddir}/lib\"\n"
    ));

    SHOULDPASS {
        run(conf, S("--cflags"), S("--libs"), S("example"), E);
    }
    EXPECT(
        "-I$(top_builddir)/include "
        "-IC:/Program\\ Files\\ \\(x86\\)/Contoso/include "
        "-L$(top_builddir)/lib\n"
    );

    conf.top_builddir = S("U:/falstaffj$/Henry IV (Part 1)");
    SHOULDPASS {
        run(conf, S("--libs"), S("example"), E);
    }
    EXPECT(
        "-LU:/falstaffj\\$/Henry\\ IV\\ \\(Part\\ 1\\)/lib\n"
    );
}

static void test_error_messages(arena a)
{
    // Check that error messages mention important information
    config conf = newtest_(a, S("error messages"));
    newfile_(&conf, S("/usr/lib/pkgconfig/badpkg.pc"), S(
        PCHDR
        "Requires: < 1\n"
    ));
    newfile_(&conf, S("/usr/lib/pkgconfig/goodpkg.pc"), S(
        PCHDR
        "Requires: badpkg\n"
    ));
    newfile_(&conf, S("/usr/lib/pkgconfig/missingversion.pc"), S(
        PCHDR
        "Requires: pkg-config >\n"
    ));
    newfile_(&conf, S("/usr/lib/pkgconfig/toodeep.pc"), S(
        PCHDR
        "Requires: ${x}\n"
        "x = x${x}\n"
    ));
    newfile_(&conf, S("/usr/lib/pkgconfig/undefinedvar.pc"), S(
        PCHDR
        "Requires: ${whatisthis}\n"
    ));
    newfile_(&conf, S("/usr/lib/pkgconfig/dupvar.pc"), S(
        PCHDR
        "toomanydefs = a\n"
        "toomanydefs = b\n"
    ));
    newfile_(&conf, S("/usr/lib/pkgconfig/missingfield.pc"), S(
        "Name:\n"
        "Version:\n"
    ));
    newfile_(&conf, S("/usr/lib/pkgconfig/versionedpkg.pc"), S(
        "Name:\n"
        "Version: 2\n"
        "Description:\n"
    ));
    newfile_(&conf, S("/usr/lib/pkgconfig/badquotes.pc"), S(
        PCHDR
        "Cflags: ${x}\n"
        "x = -I\"\n"
    ));

    SHOULDFAIL { // should be silent
        run(conf, S("--exists"), S("nonexistingpkg"), E);
    }
    EXPECT("");
    SHOULDFAIL {
        run(conf, S("--atleast-version"), S("9"), S("nonexistingpkg"), E);
    }
    EXPECT("");

    SHOULDFAIL {
        run(conf, S("--cflags"), S("nonexistingpkg"), E);
    }
    MATCH("nonexistingpkg");

    SHOULDFAIL {  // direct
        run(conf, S("--cflags"), S("badpkg"), E);
    }
    MATCH("badpkg");

    SHOULDFAIL {  // indirect
        run(conf, S("--cflags"), S("goodpkg"), E);
    }
    MATCH("badpkg");

    SHOULDFAIL {
        run(conf, S("--cflags"), S("missingversion"), E);
    }
    MATCH("missingversion");

    SHOULDFAIL {
        run(conf, S("--cflags"), S("toodeep"), E);
    }
    MATCH("toodeep");

    SHOULDFAIL {
        run(conf, S("--cflags"), S("undefinedvar"), E);
    }
    MATCH("undefinedvar");
    MATCH("whatisthis");

    SHOULDFAIL {
        run(conf, S("--cflags"), S("dupvar"), E);
    }
    MATCH("dupvar");
    MATCH("toomanydefs");

    SHOULDFAIL {
        run(conf, S("--cflags"), S("missingfield"), E);
    }
    MATCH("missingfield");
    MATCH("Description");

    SHOULDFAIL {
        run(conf, S("--cflags"), S("versionedpkg = 1"), E);
    }
    MATCH("versionedpkg");
    MATCH("'1'");
    MATCH("'2'");

    SHOULDFAIL {
        run(conf, S("--cflags"), S("badquotes"), E);
    }
    MATCH("unmatched");
    MATCH("badquotes");
}

static void printi32_(u8buf *out, i32 x)
{
    u8  buf[32];
    u8 *e = buf + countof(buf);
    u8 *p = e;
    do {
        *--p = (u8)(x%10) + '0';
    } while (x /= 10);
    prints8(out, s8span(p, e));
}

static void test_manyvars(arena a)
{
    // Stresses the hash-trie-backed package environment
    config conf = newtest_(a, S("many variables"));
    newfile_(&conf, S("manyvars.pc"), S(""));  // allocate empty file
    i32 nvars = 10000;

    for (i32 i = 0; i < nvars; i += 197) {
        config temp = conf;
        u8 prefix = 'a' + (u8)(i%26);

        // Write a fresh .pc file into the virtual "manyvars.pc" with a
        // rotated variable order and prefix to perturb the package Env.
        u8buf pc = newmembuf(&temp.perm);
        prints8(&pc, S(PCHDR));
        for (i32 v = 0; v < nvars; v++) {
            i32 vnum = (v + i) % nvars;
            printu8(&pc, prefix);
            printi32_(&pc, vnum);
            printu8(&pc, '=');
            printu8(&pc, 'A' + (u8)(vnum%26));
            printu8(&pc, '\n');
        }
        newfile_(&temp, S("manyvars.pc"), finalize(&pc));  // overwrite

        // Probe a variable to test the environment
        u8buf mem = newmembuf(&temp.perm);
        printu8(&mem, prefix);
        printi32_(&mem, i);
        printu8(&mem, 0);
        s8 var = finalize(&mem);
        SHOULDPASS {
            run(temp, S("manyvars.pc"), S("--variable"), var, E);
        }
        u8 expect[] = {'A' + (u8)(i%26), '\n', 0};
        EXPECT(expect);
    }
}

static void test_lol(arena a)
{
    config conf = newtest_(a, S("a billion laughs"));
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
    MATCH("out of memory");
}

static arena newarena_(iz cap)
{
    arena arena = {0};
    arena.beg = malloc(cap);
    if (!arena.beg) {
        __builtin_trap();
    }
    arena.end = arena.beg + cap;
    return arena;
}

int main(void)
{
    arena a = newarena_(1<<21);

    test_noargs(a);
    test_dashdash(a);
    test_modversion(a);
    test_versioncheck(a);
    test_versionorder(a);
    test_overrides(a);
    test_maximum_traverse_depth(a);
    test_private_cflags(a);
    test_private_transitive(a);
    test_revealed_transitive(a);
    test_syspaths(a);
    test_libsorder(a);
    test_staticorder(a);
    test_windows(a);
    test_parens(a);
    test_error_messages(a);
    test_manyvars(a);
    test_lol(a);

    puts("all tests pass");
    return 0;
}
