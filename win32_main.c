// Mingw-w64 Win32 platform layer for u-config
// $ cc -nostartfiles -o pkg-config win32_main.c
// This is free and unencumbered software released into the public domain.
#include "u-config.c"
#include "miniwin32.h"
#include "cmdline.c"

#ifndef PKG_CONFIG_PREFIX
#  define PKG_CONFIG_PREFIX
#endif

static b32 error_is_console = 0;

static arena newarena_(void)
{
    arena arena = {0};
    size cap = (size)1<<22;
    i32 type = MEM_COMMIT | MEM_RESERVE;
    arena.beg = VirtualAlloc(0, cap, type, PAGE_READWRITE);
    if (!arena.beg) {
        arena.beg = (byte *)16;  // aligned, non-null, zero-size arena
        cap = 0;
    }
    arena.end = arena.beg + cap;
    return arena;
}

static s8 fromwide_(arena *perm, c16 *w, i32 wlen)
{
    // NOTE: consider replacing the Win32 UTF-8 encoder/decoder with an
    // embedded WTF-8 encoder/decoder
    i32 len = WideCharToMultiByte(CP_UTF8, 0, w, wlen, 0, 0, 0, 0);
    s8 s = news8(perm, len);
    WideCharToMultiByte(CP_UTF8, 0, w, wlen, s.s, len, 0, 0);
    return s;
}

static s8 fromenv_(arena *perm, c16 *name)
{
    // NOTE: maximum environment variable size is 2**15-1, so this
    // cannot fail if the variable actually exists
    static c16 w[1<<15];
    // NOTE: A zero return is either an empty variable or an unset
    // variable. Only GetLastError can distinguish them, but the first
    // case does not clear the last error. Therefore we must clear the
    // error before GetEnvironmentVariable in order to tell them apart.
    SetLastError(0);
    i32 wlen = GetEnvironmentVariableW(name, w, countof(w));
    if (!wlen && ERROR_ENVVAR_NOT_FOUND==GetLastError()) {
        s8 r = {0};
        return r;
    }
    return fromwide_(perm, w, wlen);
}

static s8 installdir_(arena *perm)
{
    c16 exe[MAX_PATH];
    i32 len = GetModuleFileNameW(0, exe, countof(exe));
    for (size i = 0; i < len; i++) {
        if (exe[i] == '\\') {
            exe[i] = '/';
        }
    }
    s8 path = fromwide_(perm, exe, len);
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
    size len = base.len + lib.len + delim.len + base.len + share.len;
    s8 s = news8(perm, len);
    s8 r = s8copy(s, base);
       r = s8copy(r, lib);
       r = s8copy(r, delim);
       r = s8copy(r, base);
           s8copy(r, share);
    return s;
}

static s8 fromcstr_(u8 *z)
{
    s8 s = {0};
    s.s = z;
    if (s.s) {
        for (; s.s[s.len]; s.len++) {}
    }
    return s;
}

static config *newconfig_()
{
    config *conf = 0;
    arena perm = newarena_();
    conf = new(&perm, config, 1);
    conf->perm = perm;
    return conf;
}

__attribute((force_align_arg_pointer))
void mainCRTStartup(void)
{
    config *conf = newconfig_();
    conf->delim = ';';
    conf->define_prefix = 1;
    arena *perm = &conf->perm;

    i32 dummy;
    i32 err = GetStdHandle(STD_ERROR_HANDLE);
    error_is_console = GetConsoleMode(err, &dummy);

    u8 **argv = new(perm, u8 *, CMDLINE_ARGV_MAX);
    c16 *cmdline = GetCommandLineW();
    conf->nargs = cmdline_to_argv8(cmdline, argv) - 1;
    conf->args = new(perm, s8, conf->nargs);
    for (size i = 0; i < conf->nargs; i++) {
        conf->args[i] = fromcstr_(argv[i+1]);
    }

    s8 base = installdir_(perm);
    conf->envpath = fromenv_(perm, L"PKG_CONFIG_PATH");
    conf->fixedpath = fromenv_(perm, L"PKG_CONFIG_LIBDIR");
    if (!conf->fixedpath.s) {
        s8 lib   = S(PKG_CONFIG_PREFIX "/lib/pkgconfig");
        s8 share = S(PKG_CONFIG_PREFIX "/share/pkgconfig");
        conf->fixedpath = makepath_(perm, base, lib, share);
    }
    conf->top_builddir = fromenv_(perm, L"PKG_CONFIG_TOP_BUILD_DIR");
    conf->sys_incpath  = append2_(perm, base, S(PKG_CONFIG_PREFIX "/include"));
    conf->sys_libpath  = append2_(perm, base, S(PKG_CONFIG_PREFIX "/lib"));
    conf->print_sysinc = fromenv_(perm, L"PKG_CONFIG_ALLOW_SYSTEM_CFLAGS");
    conf->print_syslib = fromenv_(perm, L"PKG_CONFIG_ALLOW_SYSTEM_LIBS");

    uconfig(conf);
    ExitProcess(0);
    assert(0);
}

static filemap os_mapfile(arena *perm, s8 path)
{
    assert(path.len > 0);
    assert(!path.s[path.len-1]);

    filemap r = {0};

    c16 wpath[MAX_PATH];
    i32 wlen = MultiByteToWideChar(
        CP_UTF8, 0, path.s, (i32)path.len, wpath, countof(wpath)
    );
    if (!wlen) {
        r.status = filemap_NOTFOUND;
        return r;
    }

    i32 h = CreateFileW(
        wpath,
        GENERIC_READ,
        FILE_SHARE_ALL,
        0,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        0
    );
    if (h == INVALID_HANDLE_VALUE) {
        r.status = filemap_NOTFOUND;
        return r;
    }

    r.data.s = (u8 *)perm->beg;
    size cap = perm->end - perm->beg;
    while (r.data.len < cap) {
        i32 max = (i32)1<<30;
        i32 len = cap-r.data.len<max ? (i32)(cap-r.data.len) : max;
        ReadFile(h, r.data.s+r.data.len, len, &len, 0);
        if (len < 1) {
            break;
        }
        r.data.len += len;
    }
    CloseHandle(h);

    if (r.data.len == cap) {
        // If it filled all available space, assume the file is too large.
        r.status = filemap_READERR;
        return r;
    }

    perm->beg += r.data.len;
    r.status = filemap_OK;
    return r;
}

static void os_fail(void)
{
    ExitProcess(1);
    assert(0);
}

static void os_write(i32 fd, s8 s)
{
    assert((i32)s.len == s.len);
    assert(fd==1 || fd==2);
    i32 id = fd==1 ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE;
    i32 h = GetStdHandle(id);

    if (fd==2 && error_is_console) {
        static c16 tmp[1<<12];
        i32 len = MultiByteToWideChar(
            CP_UTF8, 0, s.s, (i32)s.len, tmp, countof(tmp)
        );
        if (len) {
            i32 dummy;
            WriteConsoleW(h, tmp, len, &dummy, 0);
            return;
        }
        // Too large, fallback to WriteFile
    }

    i32 dummy;
    b32 r = WriteFile(h, s.s, (i32)s.len, &dummy, 0);
    if (!r) {
        os_fail();
    }
}
