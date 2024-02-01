// MSVC Win32 platform layer for u-config
// $ cl /GS- /O2 /Os /Fe:pkg-config.exe msvc_main.c
// This is free and unencumbered software released into the public domain.
#pragma comment(linker, "/subsystem:console")
#pragma comment(lib, "libvcruntime.lib")
#pragma comment(lib, "kernel32.lib")

#ifdef _WIN64
#  define __PTRDIFF_TYPE__       __int64
#  define __SIZE_TYPE__          unsigned __int64
#else
#  define __PTRDIFF_TYPE__       __int32
#  define __SIZE_TYPE__          unsigned __int32
#endif
#define __builtin_unreachable()  __assume(0)
#define __attribute(x)

#include "win32_main.c"
