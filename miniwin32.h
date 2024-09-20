// Win32 types, constants, and declarations (replaces windows.h)
// This is free and unencumbered software released into the public domain.

typedef __PTRDIFF_TYPE__ iptr;
typedef __SIZE_TYPE__    uptr;
typedef unsigned short   char16_t;
typedef char16_t         c16;

enum {
    FILE_ATTRIBUTE_NORMAL = 0x80,

    FILE_SHARE_ALL = 7,

    GENERIC_READ = (i32)0x80000000,

    INVALID_HANDLE_VALUE = -1,

    MEM_COMMIT  = 0x1000,
    MEM_RESERVE = 0x2000,

    OPEN_EXISTING = 3,

    PAGE_READWRITE = 4,

    STD_OUTPUT_HANDLE = -11,
    STD_ERROR_HANDLE  = -12,
};

#define W32(r) __declspec(dllimport) r __stdcall
W32(b32)    CloseHandle(iptr);
W32(i32)    CreateFileW(c16 *, i32, i32, uptr, i32, i32, i32);
W32(void)   ExitProcess(i32);
W32(c16 *)  GetCommandLineW(void);
W32(b32)    GetConsoleMode(iptr, i32 *);
W32(i32)    GetEnvironmentVariableW(c16 *, c16 *, i32);
W32(i32)    GetModuleFileNameW(iptr, c16 *, i32);
W32(i32)    GetStdHandle(i32);
W32(b32)    ReadFile(iptr, u8 *, i32, i32 *, uptr);
W32(byte *) VirtualAlloc(uptr, size, i32, i32);
W32(b32)    WriteConsoleW(iptr, c16 *, i32, i32 *, uptr);
W32(b32)    WriteFile(iptr, u8 *, i32, i32 *, uptr);
