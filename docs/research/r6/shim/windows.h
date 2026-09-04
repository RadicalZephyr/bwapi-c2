#ifndef R6_WIN_SHIM
#define R6_WIN_SHIM
// Minimal Win32 shim: exactly the surface BWAPI/Client/Client.h and
// BWAPIClient/Source/Client.cpp use. Declarations only, never defined, so the
// Win32 imports appear as undefined symbols -- which is the point.
typedef void*         HANDLE;
typedef int           BOOL;
typedef unsigned long DWORD;
typedef const char*   LPCSTR;
typedef void*         LPVOID;
typedef const void*   LPCVOID;
typedef void*         LPSECURITY_ATTRIBUTES;
typedef DWORD*        LPDWORD;
typedef void*         LPOVERLAPPED;
typedef unsigned long long SIZE_T;
#define TRUE  1
#define FALSE 0
#define INVALID_HANDLE_VALUE ((HANDLE)(long long)-1)
#define FILE_MAP_WRITE 0x0002
#define FILE_MAP_READ  0x0004
#define GENERIC_READ   0x80000000
#define GENERIC_WRITE  0x40000000
#define OPEN_EXISTING  3
typedef struct _COMMTIMEOUTS {
  DWORD ReadIntervalTimeout, ReadTotalTimeoutMultiplier, ReadTotalTimeoutConstant;
  DWORD WriteTotalTimeoutMultiplier, WriteTotalTimeoutConstant;
} COMMTIMEOUTS, *LPCOMMTIMEOUTS;
extern "C" {
HANDLE OpenFileMappingA(DWORD, BOOL, LPCSTR);
LPVOID MapViewOfFile(HANDLE, DWORD, DWORD, DWORD, SIZE_T);
BOOL   UnmapViewOfFile(LPCVOID);
BOOL   CloseHandle(HANDLE);
HANDLE CreateFileA(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
BOOL   WriteFile(HANDLE, LPCVOID, DWORD, LPDWORD, LPOVERLAPPED);
BOOL   ReadFile(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
BOOL   SetCommTimeouts(HANDLE, LPCOMMTIMEOUTS);
DWORD  GetLastError(void);
}
#endif
