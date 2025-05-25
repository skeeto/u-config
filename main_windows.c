// Mingw-w64 Win32 platform layer for u-config
// $ cc -nostartfiles -o pkg-config main_windows.c
// This is free and unencumbered software released into the public domain.
#include "src/u-config.c"
#include "src/miniwin32.h"
#include "src/cmdline.c"

#ifndef PKG_CONFIG_PREFIX
#  define PKG_CONFIG_PREFIX
#endif

// For communication with os_write()
struct os {
    struct {
        iptr h;
        b32  isconsole;
        b32  err;
    } handles[3];
};

typedef struct {
    c16 *s;
    iz   len;
} s16;

static s16 s16cuthead_(s16 s, iz off)
{
    assert(off >= 0);
    assert(off <= s.len);
    s.s += off;
    s.len -= off;
    return s;
}

static arena newarena_(iz cap)
{
    arena arena = {0};
    arena.beg = VirtualAlloc(0, cap, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
    if (!arena.beg) {
        arena.beg = (byte *)16;  // aligned, non-null, zero-size arena
        cap = 0;
    }
    arena.end = arena.beg + cap;
    return arena;
}

typedef i32 char32_t;
typedef char32_t c32;

enum {
    REPLACEMENT_CHARACTER = 0xfffd
};

typedef struct {
    s8  tail;
    c32 rune;
} utf8;

static utf8 utf8decode_(s8 s)
{
    assert(s.len);
    utf8 r = {0};
    switch (s.s[0]&0xf0) {
    default  : r.rune = s.s[0];
               if (r.rune > 0x7f) break;
               r.tail = cuthead(s, 1);
               return r;
    case 0xc0:
    case 0xd0: if (s.len < 2) break;
               if ((s.s[1]&0xc0) != 0x80) break;
               r.rune = (i32)(s.s[0]&0x1f) << 6 |
                        (i32)(s.s[1]&0x3f) << 0;
               if (r.rune < 0x80) break;
               r.tail = cuthead(s, 2);
               return r;
    case 0xe0: if (s.len < 3) break;
               if ((s.s[1]&0xc0) != 0x80) break;
               if ((s.s[2]&0xc0) != 0x80) break;
               r.rune = (i32)(s.s[0]&0x0f) << 12 |
                        (i32)(s.s[1]&0x3f) <<  6 |
                        (i32)(s.s[2]&0x3f) <<  0;
               if (r.rune < 0x800) break;
               if (r.rune>=0xd800 && r.rune<=0xdfff) break;
               r.tail = cuthead(s, 3);
               return r;
    case 0xf0: if (s.len < 4) break;
               if ((s.s[1]&0xc0) != 0x80) break;
               if ((s.s[2]&0xc0) != 0x80) break;
               if ((s.s[3]&0xc0) != 0x80) break;
               r.rune = (i32)(s.s[0]&0x0f) << 18 |
                        (i32)(s.s[1]&0x3f) << 12 |
                        (i32)(s.s[2]&0x3f) <<  6 |
                        (i32)(s.s[3]&0x3f) <<  0;
               if (r.rune < 0x10000) break;
               if (r.rune > 0x10ffff) break;
               r.tail = cuthead(s, 4);
               return r;
    }
    r.rune = REPLACEMENT_CHARACTER;
    r.tail = cuthead(s, 1);
    return r;
}

// Encode code point returning the output length (1-4).
static i32 utf8encode_(u8 *s, c32 rune)
{
    if (rune<0 || (rune>=0xd800 && rune<=0xdfff) || rune>0x10ffff) {
        rune = REPLACEMENT_CHARACTER;
    }
    switch ((rune >= 0x80) + (rune >= 0x800) + (rune >= 0x10000)) {
    case 0: s[0] = (u8)(0x00 | ((rune >>  0)     )); return 1;
    case 1: s[0] = (u8)(0xc0 | ((rune >>  6)     ));
            s[1] = (u8)(0x80 | ((rune >>  0) & 63)); return 2;
    case 2: s[0] = (u8)(0xe0 | ((rune >> 12)     ));
            s[1] = (u8)(0x80 | ((rune >>  6) & 63));
            s[2] = (u8)(0x80 | ((rune >>  0) & 63)); return 3;
    case 3: s[0] = (u8)(0xf0 | ((rune >> 18)     ));
            s[1] = (u8)(0x80 | ((rune >> 12) & 63));
            s[2] = (u8)(0x80 | ((rune >>  6) & 63));
            s[3] = (u8)(0x80 | ((rune >>  0) & 63)); return 4;
    }
    assert(0);
}

typedef struct {
    s16 tail;
    c32 rune;
} utf16;

static utf16 utf16decode_(s16 s)
{
    assert(s.len);
    utf16 r = {0};
    r.rune = s.s[0];
    if (r.rune>=0xdc00 && r.rune<=0xdfff) {
        goto reject;  // unpaired low surrogate
    } else if (r.rune>=0xd800 && r.rune<=0xdbff) {
        if (s.len < 2) {
            goto reject;  // missing low surrogate
        }
        i32 hi = r.rune;
        i32 lo = s.s[1];
        if (lo<0xdc00 || lo>0xdfff) {
            goto reject;  // expected low surrogate
        }
        r.rune = 0x10000 + ((hi - 0xd800)<<10) + (lo - 0xdc00);
        r.tail = s16cuthead_(s, 2);
        return r;
    }
    r.tail = s16cuthead_(s, 1);
    return r;

    reject:
    r.rune = REPLACEMENT_CHARACTER;
    r.tail = s16cuthead_(s, 1);
    return r;
}

// Encode code point returning the output length (1-2).
static i32 utf16encode_(c16 *dst, c32 rune)
{
    if (rune<0 || (rune>=0xd800 && rune<=0xdfff) || rune>0x10ffff) {
        rune = REPLACEMENT_CHARACTER;
    }
    if (rune >= 0x10000) {
        rune -= 0x10000;
        dst[0] = (c16)((rune >> 10) + 0xd800);
        dst[1] = (c16)((rune&0x3ff) + 0xdc00);
        return 2;
    }
    dst[0] = (c16)rune;
    return 1;
}

static s16 towide_(arena *perm, s8 s)
{
    iz len = 0;
    utf8 state = {0};
    state.tail = s;
    while (state.tail.len) {
        state = utf8decode_(state.tail);
        c16 tmp[2];
        len += utf16encode_(tmp, state.rune);
    }

    s16 w = {0};
    w.s = new(perm, c16, len);
    state.tail = s;
    while (state.tail.len) {
        state = utf8decode_(state.tail);
        w.len += utf16encode_(w.s+w.len, state.rune);
    }
    return w;
}

static s8 fromwide_(arena *perm, s16 w)
{
    iz len = 0;
    utf16 state = {0};
    state.tail = w;
    while (state.tail.len) {
        state = utf16decode_(state.tail);
        u8 tmp[4];
        len += utf8encode_(tmp, state.rune);
    }

    s8 s = {0};
    s.s = new(perm, u8, len);
    state.tail = w;
    while (state.tail.len) {
        state = utf16decode_(state.tail);
        s.len += utf8encode_(s.s+s.len, state.rune);
    }
    return s;
}

static s8 fromenv_(arena *perm, c16 *name)
{
    // Given no buffer, unset variables report as size 0, while empty
    // variables report as size 1 for the null terminator.
    i32 wlen = GetEnvironmentVariableW(name, 0, 0);
    if (!wlen) {
        s8 r = {0};
        return r;
    }

    // Store temporarily at the beginning of the arena.
    iz cap = (perm->end - perm->beg) / (iz)sizeof(c16);
    if (wlen > cap) {
        oom(perm->ctx);
    }
    s16 wvar = {0};
    wvar.s   = (c16 *)perm->beg;
    wvar.len = wlen - 1;
    GetEnvironmentVariableW(name, wvar.s, wlen);

    byte *save = perm->beg;
    perm->beg = (byte *)(wvar.s + wvar.len);
    s8 var = fromwide_(perm, wvar);
    perm->beg = save;

    return var;
}

// Normalize path to slashes as separators.
static s8 normalize_(s8 path)
{
    for (iz i = 0; i < path.len; i++) {
        if (path.s[i] == '\\') {
            path.s[i] = '/';
        }
    }
    return path;
}

static i32 truncsize(iz len)
{
    i32 max = 0x7fffffff;
    return len>max ? max : (i32)len;
}

typedef struct list_entry list_entry;
struct list_entry {
    list_entry *flink;
    list_entry *a;
};

typedef struct {
    u8         a[8];
    void      *b;
    list_entry c;
    list_entry in_memory_order_links;
} ldr;

typedef struct {
    u8    a[4];
    void *b;
    void *image_base_address;
    ldr  *ldr;
} peb;

typedef struct {
    c16  length;
    c16  maximum_length;
    c16 *buffer;
} unicode_string;

typedef struct {
    list_entry     a;
    list_entry     in_memory_order_link;
    list_entry     b;
    void          *image_base;
    void          *c;
    u32            d;
    unicode_string full_image_name;
} ldr_data_table_entry;

static s8 installdir_(arena *perm, peb *peb)
{
    list_entry *entry = &*peb->ldr->in_memory_order_links.flink;
    ldr_data_table_entry *ldr_entry =
        containerof(entry, ldr_data_table_entry, in_memory_order_link);
    assert(ldr_entry->image_base == peb->image_base_address);
    s16 name = {0};
    name.s   = ldr_entry->full_image_name.buffer;
    name.len = ldr_entry->full_image_name.length / 2;
    s8 path = normalize_(fromwide_(perm, name));
    return dirname(dirname(path));
}

static s8 append2_(arena *perm, s8 pre, s8 suf)
{
    s8 s = news8(perm, pre.len+suf.len);
    s8copy(s8copy(s, pre), suf);
    return s;
}

static s8 makepath_(arena *perm, s8 base, s8 lib, s8 share)
{
    s8 delim = S(";");
    iz len = base.len + lib.len + delim.len + base.len + share.len;
    s8 s = news8(perm, len);
    s8 r = s8copy(s, base);
       r = s8copy(r, lib);
       r = s8copy(r, delim);
       r = s8copy(r, base);
           s8copy(r, share);
    return s;
}

static config *newconfig_(os *ctx)
{
    arena perm = newarena_(1<<22);
    perm.ctx = ctx;
    config *conf = new(&perm, config, 1);
    conf->perm = perm;
    conf->haslisting = 1;
    return conf;
}

__attribute((force_align_arg_pointer))
void mainCRTStartup(peb *peb)
{
    os ctx[1] = {0};
    i32 dummy;
    ctx->handles[1].h         = GetStdHandle(STD_OUTPUT_HANDLE);
    ctx->handles[1].isconsole = GetConsoleMode(ctx->handles[1].h, &dummy);
    ctx->handles[2].h         = GetStdHandle(STD_ERROR_HANDLE);
    ctx->handles[2].isconsole = GetConsoleMode(ctx->handles[2].h, &dummy);

    config *conf = newconfig_(ctx);
    conf->delim = ';';
    conf->define_prefix = 1;
    arena *perm = &conf->perm;

    u8 **argv = new(perm, u8 *, CMDLINE_ARGV_MAX);
    c16 *cmdline = GetCommandLineW();
    conf->nargs = cmdline_to_argv8(cmdline, argv) - 1;
    conf->args = argv + 1;

    s8 base  = installdir_(perm, peb);
    s8 lib   = S(PKG_CONFIG_PREFIX "/lib/pkgconfig");
    s8 share = S(PKG_CONFIG_PREFIX "/share/pkgconfig");
    conf->pc_path = makepath_(perm, base, lib, share);
    conf->pc_sysincpath = append2_(perm, base, S(PKG_CONFIG_PREFIX "/include"));
    conf->pc_syslibpath = append2_(perm, base, S(PKG_CONFIG_PREFIX "/lib"));
    conf->envpath = fromenv_(perm, L"PKG_CONFIG_PATH");
    conf->fixedpath = fromenv_(perm, L"PKG_CONFIG_LIBDIR");
    if (!conf->fixedpath.s) {
        conf->fixedpath = conf->pc_path;
    }
    conf->top_builddir = fromenv_(perm, L"PKG_CONFIG_TOP_BUILD_DIR");
    conf->sys_incpath  = conf->pc_sysincpath;
    conf->sys_libpath  = conf->pc_syslibpath;
    conf->print_sysinc = fromenv_(perm, L"PKG_CONFIG_ALLOW_SYSTEM_CFLAGS");
    conf->print_syslib = fromenv_(perm, L"PKG_CONFIG_ALLOW_SYSTEM_LIBS");

    // Reduce backslash occurrences in outputs
    normalize_(conf->envpath);
    normalize_(conf->fixedpath);
    normalize_(conf->top_builddir);

    uconfig(conf);
    ExitProcess(ctx->handles[1].err || ctx->handles[2].err);
    assert(0);
}

static filemap os_mapfile(os *ctx, arena *perm, s8 path)
{
    assert(ctx);
    assert(path.len > 0);
    assert(!path.s[path.len-1]);

    filemap r = {0};

    i32 handle = 0;
    {
        arena scratch = *perm;
        s16 wpath = towide_(&scratch, path);
        handle = CreateFileW(
            wpath.s,
            GENERIC_READ,
            FILE_SHARE_ALL,
            0,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            0
        );
        if (handle == INVALID_HANDLE_VALUE) {
            r.status = filemap_NOTFOUND;
            return r;
        }
    }

    r.data.s = (u8 *)perm->beg;
    iz cap = perm->end - perm->beg;
    while (r.data.len < cap) {
        i32 len = truncsize(cap - r.data.len);
        ReadFile(handle, r.data.s+r.data.len, len, &len, 0);
        if (len < 1) {
            break;
        }
        r.data.len += len;
    }
    CloseHandle(handle);

    if (r.data.len == cap) {
        // If it filled all available space, assume the file is too large.
        r.status = filemap_READERR;
        return r;
    }

    perm->beg += r.data.len;
    r.status = filemap_OK;
    return r;
}

static s8node *os_listing(os *ctx, arena *a, s8 path)
{
    assert(ctx);
    assert(path.len > 0);
    assert(!path.s[path.len-1]);

    // NOTE: will allocate while holding this handle
    iptr     handle = -1;
    finddata fd     = {0};
    {
        arena scratch = *a;
        u8buf buf = newmembuf(&scratch);
        prints8(&buf, cuttail(path, 1));
        prints8(&buf, S("\\*.pc\0"));
        s8  glob = finalize(&buf);
        s16 wide = towide_(&scratch, glob);
        handle = FindFirstFileW(wide.s, &fd);
        if (handle == -1) {
            return 0;
        }
    }

    s8list files = {0};
    do {
        s16 name = {0};
        name.s = fd.name;
        for (; name.s[name.len]; name.len++) {}
        append(&files, fromwide_(a, name), a);
    } while (FindNextFileW(handle, &fd));

    FindClose(handle);
    return files.head;
}

static void os_fail(os *ctx)
{
    assert(ctx);
    ExitProcess(1);
    assert(0);
}

typedef struct {
    c16  buf[1<<8];
    iptr handle;
    i32  len;
    b32  err;
} u16buf;

static void flushconsole_(u16buf *b)
{
    if (!b->err && b->len) {
        i32 dummy;
        b->err = !WriteConsoleW(b->handle, b->buf, b->len, &dummy, 0);
    }
    b->len = 0;
}

static void printc32_(u16buf *b, c32 rune)
{
    if (b->len > countof(b->buf)-2) {
        flushconsole_(b);
    }
    b->len += utf16encode_(b->buf+b->len, rune);
}

static void os_write(os *ctx, i32 fd, s8 s)
{
    assert((i32)s.len == s.len);  // NOTE: assume it's not a huge buffer
    assert(fd==1 || fd==2);

    b32 *err = &ctx->handles[fd].err;
    if (*err) {
        return;
    }

    iptr handle = ctx->handles[fd].h;
    if (ctx->handles[fd].isconsole) {
        // NOTE: There is a small chance that a multi-byte code point
        // spans flushes from the application. With no decoder state
        // tracked between os_write calls, this will mistranslate for
        // console outputs. The application could avoid such flushes,
        // which would require a distinct "close" flush before exits.
        //
        // Alternatively, the platform layer could detect truncated
        // encodings and buffer up to 3 bytes between calls. This buffer
        // would need to be flushed on exit by the platform.
        //
        // The primary use case for u-config is non-console outputs into
        // a build system, which will not experience this issue. Console
        // output is mainly for human-friendly debugging, so the risk is
        // acceptable.
        u16buf b = {0};
        b.handle = handle;
        utf8 state = {0};
        state.tail = s;
        while (state.tail.len) {
            state = utf8decode_(state.tail);
            printc32_(&b, state.rune);
        }
        flushconsole_(&b);
        *err = b.err;
    } else {
        i32 dummy;
        *err = !WriteFile(handle, s.s, (i32)s.len, &dummy, 0);
    }
}
