#include "shim/windows.h"
// Link-only stubs so the closure can be linked and exercised on Linux.
extern "C" {
HANDLE OpenFileMappingA(DWORD,BOOL,LPCSTR){return 0;}
LPVOID MapViewOfFile(HANDLE,DWORD,DWORD,DWORD,SIZE_T){return 0;}
BOOL   UnmapViewOfFile(LPCVOID){return 1;}
BOOL   CloseHandle(HANDLE){return 1;}
HANDLE CreateFileA(LPCSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE){return INVALID_HANDLE_VALUE;}
BOOL   WriteFile(HANDLE,LPCVOID,DWORD,LPDWORD,LPOVERLAPPED){return 0;}
BOOL   ReadFile(HANDLE,LPVOID,DWORD,LPDWORD,LPOVERLAPPED){return 0;}
BOOL   SetCommTimeouts(HANDLE,LPCOMMTIMEOUTS){return 1;}
DWORD  GetLastError(void){return 0;}
}
