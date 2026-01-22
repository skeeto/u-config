// Win32 types, constants, and declarations (replaces windows.h)
// This is free and unencumbered software released into the public domain.

#define containerof(ptr, type, member) \
    ((type *)((unsigned char *)(ptr) - offsetof(type, member)))

typedef ptrdiff_t       iptr;
typedef size_t          uptr;
typedef unsigned short  char16_t;
typedef char16_t        c16;

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

typedef struct {
    i32 attr;
    u32 create[2], access[2], write[2];
    u32 size[2];
    u32 reserved1[2];
    c16 name[260];
    c16 altname[14];
    u32 reserved2[2];
} finddata;

#define W32(r) __declspec(dllimport) r __stdcall
W32(b32)    CloseHandle(iptr);
W32(i32)    CreateFileW(c16 *, i32, i32, uptr, i32, i32, i32);
W32(void)   ExitProcess(i32);
W32(b32)    FindClose(iptr);
W32(iptr)   FindFirstFileW(c16 *, finddata *);
W32(b32)    FindNextFileW(iptr, finddata *);
W32(c16 *)  GetCommandLineW(void);
W32(b32)    GetConsoleMode(iptr, i32 *);
W32(i32)    GetEnvironmentVariableW(c16 *, c16 *, i32);
W32(iptr)   GetStdHandle(i32);
W32(b32)    ReadFile(iptr, u8 *, i32, i32 *, uptr);
W32(byte *) VirtualAlloc(uptr, iz, i32, i32);
W32(b32)    WriteConsoleW(iptr, c16 *, i32, i32 *, uptr);
W32(b32)    WriteFile(iptr, u8 *, i32, i32 *, uptr);
