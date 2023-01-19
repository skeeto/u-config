// Win32 platform layer for u-config
// This is free and unencumbered software released into the public domain.
#include "u-config.c"
#include <windows.h>

#ifndef PKG_CONFIG_PREFIX
#  define PKG_CONFIG_PREFIX
#endif

#ifdef _MSC_VER
  #define ENTRYPOINT
  #pragma comment(lib, "kernel32.lib")
  #pragma comment(lib, "shell32.lib")
  #pragma comment(linker, "/subsystem:console")
  #pragma function(memset)
  void *memset(void *d, int c, size_t n) { __stosb(d, (BYTE)c, n); return d; }
#elif __GNUC__
  #define ENTRYPOINT __attribute__((externally_visible))
  // NOTE: Required for at least -O3. Placing it in its own section
  // allows it to be ommitted via -Wl,--gc-sections when unused.
  __attribute__((section(".text.memcpy")))
  void *memcpy(void *d, const void *s, size_t n)
  {
      // NOTE: polyglot x86 and x64 inline assembly
      void *r = d;
      __asm volatile (
          "rep movsb"
          : "=D"(d), "=S"(s), "=c"(n)
          : "0"(d), "1"(s), "2"(n)
          : "memory"
      );
      return r;
  }
#endif

static Bool error_is_console = 0;

static Arena newarena_(void)
{
    Arena arena = {0};
    Size cap = 1<<28;
    #if DEBUG
    cap = 1<<21;
    #endif
    arena.mem.s = VirtualAlloc(0, cap, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
    arena.mem.len = arena.mem.s ? cap : 0;
    #ifdef DEBUG
    fillstr(arena.mem, 0xa5);
    #endif
    return arena;
}

static Str fromwide_(Arena *a, WCHAR *w, Size wlen)
{
    // NOTE: consider replacing the Win32 UTF-8 encoder/decoder with an
    // embedded WTF-8 encoder/decoder
    int len = WideCharToMultiByte(CP_UTF8, 0, w, wlen, 0, 0, 0, 0);
    Str s = newstr(a, len);
    WideCharToMultiByte(CP_UTF8, 0, w, wlen, (char *)s.s, s.len, 0, 0);
    return s;
}

static Str fromenv_(Arena *a, WCHAR *name)
{
    // NOTE: maximum environment variable size is 2**15-1, so this
    // cannot fail if the variable actually exists
    static WCHAR w[1<<15];
    DWORD wlen = GetEnvironmentVariableW(name, w, sizeof(w));
    if (!wlen) {
        Str r = {0};
        return r;
    }
    return fromwide_(a, w, wlen);
}

static Str installdir_(Arena *a)
{
    WCHAR exe[MAX_PATH];
    Size len = GetModuleFileNameW(0, exe, MAX_PATH);
    for (Size i = 0; i < len; i++) {
        if (exe[i] == '\\') {
            exe[i] = '/';
        }
    }
    Str path = fromwide_(a, exe, len);
    return dirname(dirname(path));
}

static Str makepath_(Arena *a, Str base, Str lib, Str share)
{
    Str delim = S(";");
    Size len  = base.len + lib.len + delim.len + base.len + share.len;
    Str s = newstr(a, len);
    Str r = copy(s, base);
        r = copy(r, lib);
        r = copy(r, delim);
        r = copy(r, base);
            copy(r, share);
    return s;
}

ENTRYPOINT
int mainCRTStartup(void)
{
    Config conf = {0};
    conf.delim = ';';
    conf.arena = newarena_();
    Arena *a = &conf.arena;

    DWORD dummy;
    HANDLE err = GetStdHandle(STD_ERROR_HANDLE);
    error_is_console = GetConsoleMode(err, &dummy);

    // NOTE: consider using a custom, embedded command line parser in
    // order to avoid linking shell32.dll
    int argc;
    WCHAR **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    argc--;
    argv++;
    conf.nargs = argc;
    conf.args = allocarray(a, SIZEOF(Str), conf.nargs);
    for (int i = 0; i < argc; i++) {
        Size len = 0;
        for (; argv[i][len]; len++) {}
        conf.args[i] = fromwide_(a, argv[i], len);
    }

    conf.envpath = fromenv_(a, L"PKG_CONFIG_PATH");
    conf.fixedpath = fromenv_(a, L"PKG_CONFIG_LIBDIR");
    if (!conf.fixedpath.s) {
        Str base  = installdir_(a);
        Str lib   = S(PKG_CONFIG_PREFIX "/lib/pkgconfig");
        Str share = S(PKG_CONFIG_PREFIX "/share/pkgconfig");
        conf.fixedpath = makepath_(a, base, lib, share);
    }

    conf.top_builddir  = fromenv_(a, L"PKG_CONFIG_TOP_BUILD_DIR");
    if (!conf.top_builddir.s) {
        conf.top_builddir = S("$(top_builddir)");
    }

    appmain(conf);
    ExitProcess(0);
}

static MapFileResult os_mapfile(Arena *a, Str path)
{
    (void)a;
    ASSERT(path.len > 0);
    ASSERT(!path.s[path.len-1]);

    WCHAR wpath[MAX_PATH];
    int wlen = MultiByteToWideChar(
        CP_UTF8, 0, (char *)path.s, path.len, wpath, MAX_PATH
    );
    if (!wlen) {
        MapFileResult r = {.status=MapFile_NOTFOUND};
        return r;
    }

    HANDLE h = CreateFileW(
        wpath,
        GENERIC_READ,
        FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        0,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        0
    );
    if (h == INVALID_HANDLE_VALUE) {
        MapFileResult r = {.status=MapFile_NOTFOUND};
        return r;
    }

    DWORD hi, lo = GetFileSize(h, &hi);
    if (hi || lo>Size_MAX) {
        CloseHandle(h);
        MapFileResult r = {.status=MapFile_READERR};
        return r;
    }

    HANDLE *map = CreateFileMapping(h, 0, PAGE_READONLY, 0, lo, 0);
    CloseHandle(h);
    if (!map) {
        MapFileResult r = {.status=MapFile_READERR};
        return r;
    }

    void *p = MapViewOfFile(map, FILE_MAP_READ, 0, 0, lo);
    CloseHandle(map);
    if (!p) {
        MapFileResult r = {.status=MapFile_READERR};
        return r;
    }

    MapFileResult r = {{p, (Size)lo}, MapFile_OK};
    return r;
}

static void os_fail(void)
{
    ExitProcess(1);
}

static void os_write(int fd, Str s)
{
    ASSERT(fd==1 || fd==2);
    DWORD id = fd==1 ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE;
    HANDLE h = GetStdHandle(id);
    DWORD n;

    if (fd==2 && error_is_console) {
        static WCHAR tmp[1<<12];
        int len = MultiByteToWideChar(
            CP_UTF8, 0, (char *)s.s, s.len, tmp, sizeof(tmp)
        );
        if (len) {
            WriteConsoleW(h, tmp, len, &n, 0);
            return;
        }
        // Too large, fallback to WriteFile
    }

    BOOL r = WriteFile(h, s.s, s.len, &n, 0);
    if (!r || (Size)n!=s.len) {
        os_fail();
    }
}
