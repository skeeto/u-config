// Win32 platform layer for u-config
// This is free and unencumbered software released into the public domain.
#include "u-config.c"
#include "cmdline.c"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifndef PKG_CONFIG_PREFIX
#  define PKG_CONFIG_PREFIX
#endif

#ifdef _MSC_VER
  #define ENTRYPOINT
  #pragma comment(lib, "kernel32.lib")
  #pragma comment(linker, "/subsystem:console")
  #pragma function(memset)
  void *memset(void *d, int c, size_t n) { __stosb(d, (BYTE)c, n); return d; }
#elif __GNUC__
  #define ENTRYPOINT __attribute__((externally_visible))
  // NOTE: These functions are required at higher GCC optimization
  // levels. Placing them in their own section allows them to be
  // ommitted via -Wl,--gc-sections when unused.
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
  __attribute__((section(".text.strlen")))
  size_t strlen(const char *s)
  {
      const char *b = s;
      __asm("repne scasb" : "=D"(s) : "0"(s), "a"(0), "c"((size_t)-1));
      return s - b - 1;
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
    shredfree(&arena);
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

static Str append2_(Arena *a, Str pre, Str suf)
{
    Str s = newstr(a, pre.len+suf.len);
    copy(copy(s, pre), suf);
    return s;
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

static Str fromcstr_(char *z)
{
    Str s = {(Byte *)z, 0};
    if (s.s) {
        for (; s.s[s.len]; s.len++) {}
    }
    return s;
}

ENTRYPOINT
int mainCRTStartup(void)
{
    Config conf = {0};
    conf.delim = ';';
    conf.define_prefix = 1;
    conf.arena = newarena_();
    Arena *a = &conf.arena;

    DWORD dummy;
    HANDLE err = GetStdHandle(STD_ERROR_HANDLE);
    error_is_console = GetConsoleMode(err, &dummy);

    char **argv = allocarray(a, SIZEOF(*argv), CMDLINE_ARGV_MAX);
    conf.nargs = cmdline_to_argv8(GetCommandLineW(), argv) - 1;
    conf.args = allocarray(a, SIZEOF(Str), conf.nargs);
    for (Size i = 0; i < conf.nargs; i++) {
        conf.args[i] = fromcstr_(argv[i+1]);
    }

    Str base = installdir_(a);
    conf.envpath = fromenv_(a, L"PKG_CONFIG_PATH");
    conf.fixedpath = fromenv_(a, L"PKG_CONFIG_LIBDIR");
    if (!conf.fixedpath.s) {
        Str lib   = S(PKG_CONFIG_PREFIX "/lib/pkgconfig");
        Str share = S(PKG_CONFIG_PREFIX "/share/pkgconfig");
        conf.fixedpath = makepath_(a, base, lib, share);
    }
    conf.top_builddir = fromenv_(a, L"PKG_CONFIG_TOP_BUILD_DIR");
    conf.sys_incpath  = append2_(a, base, S(PKG_CONFIG_PREFIX "/include"));
    conf.sys_libpath  = append2_(a, base, S(PKG_CONFIG_PREFIX "/lib"));

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
        MapFileResult r = {{0, 0}, MapFile_NOTFOUND};
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
        MapFileResult r = {{0, 0}, MapFile_NOTFOUND};
        return r;
    }

    DWORD hi, lo = GetFileSize(h, &hi);
    if (hi || lo>Size_MAX) {
        CloseHandle(h);
        MapFileResult r = {{0, 0}, MapFile_READERR};
        return r;
    } else if (!lo) {
        CloseHandle(h);
        // Cannot map an empty file, so use the arena for a zero-size
        // allocation, distinguishing it from a null string.
        MapFileResult r = {newstr(a, 0), MapFile_OK};
        return r;
    }

    HANDLE map = CreateFileMappingA(h, 0, PAGE_READONLY, 0, lo, 0);
    CloseHandle(h);
    if (!map) {
        MapFileResult r = {{0, 0}, MapFile_READERR};
        return r;
    }

    void *p = MapViewOfFile(map, FILE_MAP_READ, 0, 0, lo);
    CloseHandle(map);
    if (!p) {
        MapFileResult r = {{0, 0}, MapFile_READERR};
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
