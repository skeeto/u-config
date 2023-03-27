// Win32 API: windows.h replacement, halves build times

typedef int BOOL;
typedef void *HANDLE;
typedef unsigned DWORD;
#if __GNUC__  // in MSVC size_t is a built-in type
  typedef __SIZE_TYPE__ size_t;
#endif
#if !__cplusplus || (_MSC_VER && !_NATIVE_WCHAR_T_DEFINED)
  // NOTE: wchar_t is a built-in type in C++, except older versions of
  // Visual Studio are not so C++-compliant without /Zc:wchar_t.
  typedef unsigned short wchar_t;
#endif

#define CP_UTF8 65001

#define FILE_ATTRIBUTE_NORMAL 0x80

#define FILE_MAP_READ 4

#define FILE_SHARE_DELETE 4
#define FILE_SHARE_READ   1
#define FILE_SHARE_WRITE  2

#define GENERIC_READ 0x80000000

#define INVALID_HANDLE_VALUE ((HANDLE)-1)

#define MAX_PATH 260

#define MEM_COMMIT  0x1000
#define MEM_RESERVE 0x2000

#define OPEN_EXISTING 3

#define PAGE_READONLY  2
#define PAGE_READWRITE 4

#define STD_ERROR_HANDLE  -12
#define STD_OUTPUT_HANDLE -11

#ifdef __cplusplus
extern "C" {
#endif
__declspec(dllimport) HANDLE __stdcall CreateFileW(
    wchar_t *, DWORD, DWORD, void *, DWORD, DWORD, HANDLE);
__declspec(dllimport) BOOL __stdcall CloseHandle(HANDLE);
__declspec(dllimport) HANDLE __stdcall CreateFileMappingA(
    HANDLE, void *, DWORD, DWORD, DWORD, char *);
__declspec(dllimport) int __stdcall MultiByteToWideChar(
    unsigned, DWORD, char *, int, wchar_t *, int);
__declspec(dllimport) wchar_t *__stdcall GetCommandLineW(void);
__declspec(dllimport) BOOL __stdcall GetConsoleMode(HANDLE, DWORD *);
__declspec(dllimport) DWORD __stdcall GetEnvironmentVariableW(
    const wchar_t *, wchar_t *, DWORD);
__declspec(dllimport) DWORD __stdcall GetFileSize(HANDLE, DWORD *);
__declspec(dllimport) DWORD __stdcall GetModuleFileNameW(
    HANDLE, wchar_t *, DWORD);
__declspec(dllimport) HANDLE __stdcall GetStdHandle(DWORD);
__declspec(dllimport) void *__stdcall MapViewOfFile(
    HANDLE, DWORD, DWORD, DWORD, size_t);
__declspec(dllimport) void *__stdcall VirtualAlloc(
    void *, size_t, DWORD, DWORD);
__declspec(dllimport) int __stdcall WideCharToMultiByte(
    unsigned, DWORD, wchar_t *, int, char *, int, char *, BOOL *);
__declspec(dllimport) BOOL __stdcall WriteConsoleW(
    HANDLE, wchar_t *, DWORD, DWORD *, void *);
__declspec(dllimport) BOOL __stdcall WriteFile(
    HANDLE, void *, DWORD, DWORD *, void *);
__declspec(dllimport) __declspec(noreturn) void __stdcall ExitProcess(DWORD);
#ifdef __cplusplus
}
#endif
