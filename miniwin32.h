// Win32 types, constants, and declarations (replaces windows.h)
// This is free and unencumbered software released into the public domain.

typedef __SIZE_TYPE__  uptr;
typedef unsigned short char16_t;
typedef char16_t       c16;

enum {
    CP_UTF8 = 65001,

    FILE_ATTRIBUTE_NORMAL = 0x80,

    FILE_SHARE_ALL = 7,

    GENERIC_READ = 0x80000000,

    INVALID_HANDLE_VALUE = -1,

    MAX_PATH = 260,

    MEM_COMMIT  = 0x1000,
    MEM_RESERVE = 0x2000,

    OPEN_EXISTING = 3,

    PAGE_READWRITE = 4,

    STD_OUTPUT_HANDLE = -11,
    STD_ERROR_HANDLE  = -12,

    ERROR_ENVVAR_NOT_FOUND = 0xcb
};

#define W32(r) __declspec(dllimport) r __stdcall
W32(b32)    CloseHandle(uptr);
W32(i32)    CreateFileW(c16 *, i32, i32, uptr, i32, i32, i32);
W32(void)   ExitProcess(i32);
W32(c16 *)  GetCommandLineW(void);
W32(b32)    GetConsoleMode(uptr, i32 *);
W32(i32)    GetEnvironmentVariableW(c16 *, c16 *, i32);
W32(u32)    GetLastError(void);
W32(i32)    GetModuleFileNameW(uptr, c16 *, i32);
W32(i32)    GetStdHandle(i32);
W32(i32)    MultiByteToWideChar(i32, i32, u8 *, i32, c16 *, i32);
W32(b32)    ReadFile(uptr, u8 *, i32, i32 *, uptr);
W32(void)   SetLastError(u32);
W32(byte *) VirtualAlloc(uptr, size, i32, i32);
W32(i32)    WideCharToMultiByte(i32, i32, c16 *, i32, u8 *, i32, uptr, uptr);
W32(b32)    WriteConsoleW(uptr, c16 *, i32, i32 *, uptr);
W32(b32)    WriteFile(uptr, u8 *, i32, i32 *, uptr);
