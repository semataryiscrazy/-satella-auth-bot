#pragma once
#include <windows.h>
#include "source/Cfg/strenc.h"

__forceinline LPVOID DynVirtualAllocEx(HANDLE hProc, LPVOID addr, SIZE_T size, DWORD type, DWORD prot) {
    typedef LPVOID (WINAPI *FnVirtualAllocEx)(HANDLE,LPVOID,SIZE_T,DWORD,DWORD);
    static FnVirtualAllocEx _fn = (FnVirtualAllocEx)GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("kernel32.dll")), AY_OBFUSCATE("VirtualAllocEx"));
    return _fn ? _fn(hProc, addr, size, type, prot) : NULL;
}

__forceinline BOOL DynWriteProcessMemory(HANDLE hProc, LPVOID addr, LPCVOID buf, SIZE_T size, SIZE_T* written) {
    typedef BOOL (WINAPI *FnWriteProcessMemory)(HANDLE,LPVOID,LPCVOID,SIZE_T,SIZE_T*);
    static FnWriteProcessMemory _fn = (FnWriteProcessMemory)GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("kernel32.dll")), AY_OBFUSCATE("WriteProcessMemory"));
    return _fn ? _fn(hProc, addr, buf, size, written) : FALSE;
}

__forceinline HANDLE DynCreateRemoteThread(HANDLE hProc, LPSECURITY_ATTRIBUTES attr, SIZE_T stack, LPTHREAD_START_ROUTINE start, LPVOID param, DWORD flags, LPDWORD tid) {
    typedef HANDLE (WINAPI *FnCreateRemoteThread)(HANDLE,LPSECURITY_ATTRIBUTES,SIZE_T,LPTHREAD_START_ROUTINE,LPVOID,DWORD,LPDWORD);
    static FnCreateRemoteThread _fn = (FnCreateRemoteThread)GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("kernel32.dll")), AY_OBFUSCATE("CreateRemoteThread"));
    return _fn ? _fn(hProc, attr, stack, start, param, flags, tid) : NULL;
}

__forceinline HANDLE DynOpenProcess(DWORD access, BOOL inherit, DWORD pid) {
    typedef HANDLE (WINAPI *FnOpenProcess)(DWORD,BOOL,DWORD);
    static FnOpenProcess _fn = (FnOpenProcess)GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("kernel32.dll")), AY_OBFUSCATE("OpenProcess"));
    return _fn ? _fn(access, inherit, pid) : NULL;
}

__forceinline BOOL DynVirtualFreeEx(HANDLE hProc, LPVOID addr, SIZE_T size, DWORD type) {
    typedef BOOL (WINAPI *FnVirtualFreeEx)(HANDLE,LPVOID,SIZE_T,DWORD);
    static FnVirtualFreeEx _fn = (FnVirtualFreeEx)GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("kernel32.dll")), AY_OBFUSCATE("VirtualFreeEx"));
    return _fn ? _fn(hProc, addr, size, type) : FALSE;
}

__forceinline BOOL DynCloseHandle(HANDLE h) {
    typedef BOOL (WINAPI *FnCloseHandle)(HANDLE);
    static FnCloseHandle _fn = (FnCloseHandle)GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("kernel32.dll")), AY_OBFUSCATE("CloseHandle"));
    return _fn ? _fn(h) : FALSE;
}

__forceinline BOOL DynOpenProcessToken(HANDLE h, DWORD access, PHANDLE token) {
    typedef BOOL (WINAPI *FnOpenProcessToken)(HANDLE,DWORD,PHANDLE);
    static FnOpenProcessToken _fn = (FnOpenProcessToken)GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("advapi32.dll")), AY_OBFUSCATE("OpenProcessToken"));
    return _fn ? _fn(h, access, token) : FALSE;
}

__forceinline BOOL DynAdjustTokenPrivileges(HANDLE token, BOOL disable, PTOKEN_PRIVILEGES newp, DWORD len, PTOKEN_PRIVILEGES prev, PDWORD retlen) {
    typedef BOOL (WINAPI *FnAdjustTokenPrivileges)(HANDLE,BOOL,PTOKEN_PRIVILEGES,DWORD,PTOKEN_PRIVILEGES,PDWORD);
    static FnAdjustTokenPrivileges _fn = (FnAdjustTokenPrivileges)GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("advapi32.dll")), AY_OBFUSCATE("AdjustTokenPrivileges"));
    return _fn ? _fn(token, disable, newp, len, prev, retlen) : FALSE;
}

__forceinline BOOL DynLookupPrivilegeValueA(LPCSTR sys, LPCSTR name, PLUID luid) {
    typedef BOOL (WINAPI *FnLookupPrivilegeValueA)(LPCSTR,LPCSTR,PLUID);
    static FnLookupPrivilegeValueA _fn = (FnLookupPrivilegeValueA)GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("advapi32.dll")), AY_OBFUSCATE("LookupPrivilegeValueA"));
    return _fn ? _fn(sys, name, luid) : FALSE;
}
