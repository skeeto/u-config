// Win32 types, constants, and declarations (replaces windows.h)
// This is free and unencumbered software released into the public domain.

typedef __SIZE_TYPE__  uptr;
typedef unsigned short u16;
typedef unsigned short char16_t;
typedef char16_t       c16;

enum {
    FILE_ATTRIBUTE_NORMAL = 0x80,

    FILE_SHARE_ALL = 7,

    GENERIC_READ = 0x80000000,

    INVALID_HANDLE_VALUE = -1,

    MEM_COMMIT  = 0x1000,
    MEM_RESERVE = 0x2000,

    OPEN_EXISTING = 3,

    PAGE_READWRITE = 4,

    STD_OUTPUT_HANDLE = -11,
    STD_ERROR_HANDLE  = -12,
};

b32   (__stdcall *CloseHandle)(uptr);
i32   (__stdcall *CreateFileW)(c16 *, i32, i32, uptr, i32, i32, i32);
void  (__stdcall *ExitProcess)(i32);
c16  *(__stdcall *GetCommandLineW)(void);
b32   (__stdcall *GetConsoleMode)(uptr, i32 *);
i32   (__stdcall *GetEnvironmentVariableW)(c16 *, c16 *, i32);
i32   (__stdcall *GetModuleFileNameW)(uptr, c16 *, i32);
void *(__stdcall *GetProcAddress)(u8 *, u8 *);
i32   (__stdcall *GetStdHandle)(i32);
b32   (__stdcall *ReadFile)(uptr, u8 *, i32, i32 *, uptr);
byte *(__stdcall *VirtualAlloc)(uptr, size, i32, i32);
b32   (__stdcall *WriteConsoleW)(uptr, c16 *, i32, i32 *, uptr);
b32   (__stdcall *WriteFile)(uptr, u8 *, i32, i32 *, uptr);
