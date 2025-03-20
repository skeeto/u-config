// MSVC Win32 platform layer for u-config
// $ cl /GS- /O2 /Os /Fe:pkg-config.exe main_msvc.c
// This is free and unencumbered software released into the public domain.
#pragma comment(linker, "/subsystem:console")
#pragma comment(lib, "libvcruntime.lib")
#pragma comment(lib, "kernel32.lib")

#define __builtin_unreachable()  __assume(0)
#define __attribute(x)

#include "main_windows.c"
