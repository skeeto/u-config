// Merge of Linux and Windows platform layers (x86-64 only)
// $ cc -nostartfiles -e merge_entrypoint -o pkg-config.exe main_wine.c
//
// Run under Wine, it behaves like a cross toolchain pkg-config and uses
// raw Linux system calls. Run on Windows it behaves as a native Windows
// pkg-config.
//
// This is free and unencumbered software released into the public domain.
#include "src/u-config.c"


// Import Windows platform layer
#define os_fail     win32_fail
#define os_listing  win32_listing
#define os_mapfile  win32_mapfile
#define os_write    win32_write
#include "main_windows.c"
#undef os_write
#undef os_mapfile
#undef os_listing
#undef os_fail


// Import Linux platform layer
#define PKG_CONFIG_LIBDIR "/usr/x86_64-w64-mingw32/lib/pkgconfig"
#define PKG_CONFIG_SYSTEM_INCLUDE_PATH "/usr/x86_64-w64-mingw32/include"
#define PKG_CONFIG_SYSTEM_LIBRARY_PATH "/usr/x86_64-w64-mingw32/lib"
#define os_fail     linux_fail
#define os_listing  linux_listing
#define os_mapfile  linux_mapfile
#define os_write    linux_write
#define newconfig_  linux_newconfig
#include "main_linux_amd64.c"
#undef newconfig_
#undef os_write
#undef os_mapfile
#undef os_listing
#undef os_fail


// Merge the platform layers

static b32 running_on_wine()
{
    W32(void *) GetModuleHandleA(char *);
    W32(void *) GetProcAddress(void *, char *);
    return !!GetProcAddress(GetModuleHandleA("ntdll.dll"), "wine_get_version");
}

static b32 wine_detected;

static filemap os_mapfile(os *ctx, arena *a, s8 path)
{
    if (wine_detected) {
        return linux_mapfile(ctx, a, path);
    } else {
        return win32_mapfile(ctx, a, path);
    }
}

static s8node *os_listing(os *ctx, arena *a, s8 path)
{
    if (wine_detected) {
        return linux_listing(ctx, a, path);
    } else {
        return win32_listing(ctx, a, path);
    }
}

static void os_write(os *ctx, i32 fd, s8 s)
{
    if (wine_detected) {
        linux_write(ctx, fd, s);
    } else {
        win32_write(ctx, fd, s);
    }
}

static void os_fail(os *ctx)
{
    if (wine_detected) {
        linux_fail(ctx);
    } else {
        win32_fail(ctx);
    }
    __builtin_unreachable();
}

void __stdcall merge_entrypoint()
{
    wine_detected = running_on_wine();
    if (wine_detected) {
        static u8 *fakestack[CMDLINE_ARGV_MAX+1];
        c16 *cmd = GetCommandLineW();
        fakestack[0] = (u8 *)(iz)cmdline_to_argv8(cmd, fakestack+1);
        // TODO: append envp to the fake stack
        entrypoint((iz *)fakestack);
    } else {
        mainCRTStartup();
    }
}
