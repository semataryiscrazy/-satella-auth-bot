#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <tlhelp32.h>
#include <winternl.h>
#include <stdio.h>
#include "resource.h"

#define TARGET L"HD-Player.exe"
#define HOTKEY_ID 1

static wchar_t g_dllPath[MAX_PATH];
static HANDLE g_hProcess = NULL;
static LPVOID g_remoteMem = NULL;
static DWORD g_pid = 0;

// Hell's Gate: extract syscall number from ntdll stub, produce executable trampoline
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
    if (!stub) return NULL;
    int scNum = -1;
    for (int i = 0; i < 32; i++) {
        if (stub[i] == 0x4C && stub[i+1] == 0x8B && stub[i+2] == 0xD1 && stub[i+3] == 0xB8) {
            scNum = *(int*)&stub[i+4]; break;
        }
        if (stub[i] == 0xB8 && stub[i+5] == 0x0F && stub[i+6] == 0x05) {
            scNum = *(int*)&stub[i+1]; break;
        }
    }
    if (scNum < 0) return NULL;
    BYTE* code = (BYTE*)VirtualAlloc(NULL, 11, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!code) return NULL;
    code[0] = 0x4C; code[1] = 0x8B; code[2] = 0xD1; // mov r10, rcx
    code[3] = 0xB8; *(int*)&code[4] = scNum;         // mov eax, #
    code[8] = 0x0F; code[9] = 0x05;                  // syscall
    code[10] = 0xC3;                                  // ret
    return code;
}

// Syscall function pointer types
typedef NTSTATUS(NTAPI*_NtCreateThreadEx)(PHANDLE,ACCESS_MASK,PVOID,HANDLE,PVOID,PVOID,ULONG,SIZE_T,SIZE_T,PVOID);
typedef NTSTATUS(NTAPI*_NtOpenProcess)(PHANDLE,ACCESS_MASK,POBJECT_ATTRIBUTES,struct _CLIENT_ID*);
typedef NTSTATUS(NTAPI*_NtAllocateVirtualMemory)(HANDLE,PVOID*,ULONG_PTR,PSIZE_T,ULONG,ULONG);
typedef NTSTATUS(NTAPI*_NtWriteVirtualMemory)(HANDLE,PVOID,PVOID,SIZE_T,PSIZE_T);
typedef NTSTATUS(NTAPI*_NtFreeVirtualMemory)(HANDLE,PVOID*,PSIZE_T,ULONG);
typedef NTSTATUS(NTAPI*_NtClose)(HANDLE);
typedef NTSTATUS(NTAPI*_NtQueueApcThread)(HANDLE,PVOID,PVOID,PVOID,PVOID);
typedef NTSTATUS(NTAPI*_NtSuspendThread)(HANDLE,PULONG);
typedef NTSTATUS(NTAPI*_NtResumeThread)(HANDLE,PULONG);
typedef NTSTATUS(NTAPI*_NtGetContextThread)(HANDLE,PCONTEXT);
typedef NTSTATUS(NTAPI*_NtSetContextThread)(HANDLE,PCONTEXT);

static _NtCreateThreadEx MyNtCreateThreadEx;
static _NtOpenProcess MyNtOpenProcess;
static _NtAllocateVirtualMemory MyNtAllocateVirtualMemory;
static _NtWriteVirtualMemory MyNtWriteVirtualMemory;
static _NtFreeVirtualMemory MyNtFreeVirtualMemory;
static _NtClose MyNtClose;
static _NtQueueApcThread MyNtQueueApcThread;
static _NtSuspendThread MyNtSuspendThread;
static _NtResumeThread MyNtResumeThread;
static _NtGetContextThread MyNtGetContextThread;
static _NtSetContextThread MyNtSetContextThread;

static void InitSyscalls() {
    MyNtCreateThreadEx = (_NtCreateThreadEx)CreateSyscall("NtCreateThreadEx");
    MyNtOpenProcess = (_NtOpenProcess)CreateSyscall("NtOpenProcess");
    MyNtAllocateVirtualMemory = (_NtAllocateVirtualMemory)CreateSyscall("NtAllocateVirtualMemory");
    MyNtWriteVirtualMemory = (_NtWriteVirtualMemory)CreateSyscall("NtWriteVirtualMemory");
    MyNtFreeVirtualMemory = (_NtFreeVirtualMemory)CreateSyscall("NtFreeVirtualMemory");
    MyNtClose = (_NtClose)CreateSyscall("NtClose");
    MyNtQueueApcThread = (_NtQueueApcThread)CreateSyscall("NtQueueApcThread");
    MyNtSuspendThread = (_NtSuspendThread)CreateSyscall("NtSuspendThread");
    MyNtResumeThread = (_NtResumeThread)CreateSyscall("NtResumeThread");
    MyNtGetContextThread = (_NtGetContextThread)CreateSyscall("NtGetContextThread");
    MyNtSetContextThread = (_NtSetContextThread)CreateSyscall("NtSetContextThread");
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

static BOOL OpenTarget() {
    g_pid = FindPid();
    if (!g_pid) return FALSE;
    OBJECT_ATTRIBUTES oa = { sizeof(oa) };
    CLIENT_ID cid = { (HANDLE)(ULONG_PTR)g_pid, NULL };
    g_hProcess = NULL;
    if (MyNtOpenProcess) MyNtOpenProcess(&g_hProcess, PROCESS_ALL_ACCESS, &oa, &cid);
    if (!g_hProcess) g_hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, g_pid);
    if (!g_hProcess) return FALSE;
    SIZE_T sz = (wcslen(g_dllPath)+1)*sizeof(wchar_t);
    g_remoteMem = NULL;
    if (MyNtAllocateVirtualMemory)
        MyNtAllocateVirtualMemory(g_hProcess, &g_remoteMem, 0, &sz, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
    if (!g_remoteMem) g_remoteMem = VirtualAllocEx(g_hProcess, NULL, sz, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
    if (!g_remoteMem) { CloseHandle(g_hProcess); g_hProcess=NULL; return FALSE; }
    if (MyNtWriteVirtualMemory)
        MyNtWriteVirtualMemory(g_hProcess, g_remoteMem, g_dllPath, sz, NULL);
    else
        WriteProcessMemory(g_hProcess, g_remoteMem, g_dllPath, sz, NULL);
    return TRUE;
}

static void CloseTarget() {
    if (g_remoteMem && g_hProcess) {
        SIZE_T sz=0;
        if (MyNtFreeVirtualMemory) MyNtFreeVirtualMemory(g_hProcess,&g_remoteMem,&sz,MEM_RELEASE);
        else VirtualFreeEx(g_hProcess,g_remoteMem,0,MEM_RELEASE);
    }
    if (g_hProcess) { if(MyNtClose)MyNtClose(g_hProcess);else CloseHandle(g_hProcess); }
    g_hProcess=NULL; g_remoteMem=NULL;
}

// ─── Method 1: NtCreateThreadEx via direct syscall ───
static BOOL M1_CreateThread() {
    if (!MyNtCreateThreadEx || !OpenTarget()) return FALSE;
    HANDLE hT=NULL;
    NTSTATUS s=MyNtCreateThreadEx(&hT,THREAD_ALL_ACCESS,NULL,g_hProcess,LoadLibraryW,g_remoteMem,0,0,0,NULL);
    if(s>=0&&hT){WaitForSingleObject(hT,5000);if(MyNtClose)MyNtClose(hT);else CloseHandle(hT);CloseTarget();return TRUE;}
    CloseTarget(); return FALSE;
}

// ─── Method 2: QueueUserAPC via direct syscall ───
static BOOL M2_APC() {
    if (!MyNtQueueApcThread || !OpenTarget()) return FALSE;
    HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);
    BOOL ok=FALSE;
    if(snap!=INVALID_HANDLE_VALUE){
        THREADENTRY32 te={sizeof(te)};
        if(Thread32First(snap,&te))do{
            if(te.th32OwnerProcessID!=g_pid)continue;
            HANDLE ht=OpenThread(THREAD_ALL_ACCESS,FALSE,te.th32ThreadID);
            if(!ht)continue;
            if(MyNtQueueApcThread(ht,LoadLibraryW,g_remoteMem,NULL,NULL)>=0)ok=TRUE;
            if(MyNtClose)MyNtClose(ht);else CloseHandle(ht);
        }while(Thread32Next(snap,&te));
        CloseHandle(snap);
    }
    CloseTarget(); return ok;
}

// ─── Method 3: Thread hijacking via direct syscall ───
static BOOL M3_Hijack() {
    if(!MyNtSetContextThread||!MyNtSuspendThread||!MyNtGetContextThread||!MyNtResumeThread||!OpenTarget())return FALSE;
    SIZE_T sz=0x400; LPVOID sc=NULL;
    if(MyNtAllocateVirtualMemory)MyNtAllocateVirtualMemory(g_hProcess,&sc,0,&sz,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
    if(!sc)sc=VirtualAllocEx(g_hProcess,NULL,0x400,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
    if(!sc){CloseTarget();return FALSE;}
    ULONGLONG ldr=(ULONGLONG)GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"LdrLoadDll");
    if(MyNtWriteVirtualMemory){
        MyNtWriteVirtualMemory(g_hProcess,(BYTE*)sc+0x100,&ldr,8,NULL);
        MyNtWriteVirtualMemory(g_hProcess,(BYTE*)sc+0x108,g_dllPath,(wcslen(g_dllPath)+1)*2,NULL);
    }else{
        WriteProcessMemory(g_hProcess,(BYTE*)sc+0x100,&ldr,8,NULL);
        WriteProcessMemory(g_hProcess,(BYTE*)sc+0x108,g_dllPath,(wcslen(g_dllPath)+1)*2,NULL);
    }
    UNICODE_STRING us;us.Length=(USHORT)(wcslen(g_dllPath)*2);us.MaximumLength=us.Length+2;us.Buffer=(PWCH)((BYTE*)sc+0x108);
    if(MyNtWriteVirtualMemory)MyNtWriteVirtualMemory(g_hProcess,(BYTE*)sc+0x300,&us,sizeof(us),NULL);
    else WriteProcessMemory(g_hProcess,(BYTE*)sc+0x300,&us,sizeof(us),NULL);
    ULONGLONG z=0;
    if(MyNtWriteVirtualMemory)MyNtWriteVirtualMemory(g_hProcess,(BYTE*)sc+0x310,&z,8,NULL);
    else WriteProcessMemory(g_hProcess,(BYTE*)sc+0x310,&z,8,NULL);
    BYTE scb[]={
        0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x9C,0x48,0x83,0xEC,0x28,
        0x48,0xB9,0,0,0,0,0,0,0,0, 0x33,0xD2,
        0x49,0xB8,0,0,0,0,0,0,0,0, 0x45,0x33,0xC9,
        0x48,0xB8,0,0,0,0,0,0,0,0, 0xFF,0xD0,
        0x48,0x83,0xC4,0x28,0x9D,0x5F,0x5E,0x5D,0x5C,0x5B,0x5A,0x59,0x58,0xC3
    };
    *(ULONGLONG*)&scb[16]=(ULONGLONG)(BYTE*)sc+0x300;
    *(ULONGLONG*)&scb[29]=(ULONGLONG)(BYTE*)sc+0x310;
    *(ULONGLONG*)&scb[42]=ldr;
    if(MyNtWriteVirtualMemory)MyNtWriteVirtualMemory(g_hProcess,sc,scb,sizeof(scb),NULL);
    else WriteProcessMemory(g_hProcess,sc,scb,sizeof(scb),NULL);
    HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);
    BOOL ok=FALSE;
    if(snap!=INVALID_HANDLE_VALUE){
        THREADENTRY32 te={sizeof(te)};
        if(Thread32First(snap,&te))do{
            if(te.th32OwnerProcessID!=g_pid)continue;
            HANDLE ht=OpenThread(THREAD_ALL_ACCESS,FALSE,te.th32ThreadID);
            if(!ht)continue;
            if(MyNtSuspendThread(ht,NULL)<0){CloseHandle(ht);continue;}
            CONTEXT ctx;ctx.ContextFlags=CONTEXT_FULL;
            if(MyNtGetContextThread(ht,&ctx)<0){MyNtResumeThread(ht,NULL);CloseHandle(ht);continue;}
            ctx.Rsp-=8;
            if(MyNtWriteVirtualMemory)MyNtWriteVirtualMemory(g_hProcess,(LPVOID)ctx.Rsp,&ctx.Rip,8,NULL);
            else WriteProcessMemory(g_hProcess,(LPVOID)ctx.Rsp,&ctx.Rip,8,NULL);
            ctx.Rip=(ULONGLONG)sc;ctx.ContextFlags=CONTEXT_FULL;
            if(MyNtSetContextThread(ht,&ctx)>=0){MyNtResumeThread(ht,NULL);ok=TRUE;CloseHandle(ht);break;}
            MyNtResumeThread(ht,NULL);CloseHandle(ht);
        }while(Thread32Next(snap,&te));
        CloseHandle(snap);
    }
    if(!ok){SIZE_T sz2=0;if(MyNtFreeVirtualMemory)MyNtFreeVirtualMemory(g_hProcess,&sc,&sz2,MEM_RELEASE);else VirtualFreeEx(g_hProcess,sc,0,MEM_RELEASE);}
    CloseTarget(); return ok;
}

// ─── Method 4: Win32 CreateRemoteThread (last resort) ───
static BOOL M4_Win32() {
    g_pid=FindPid(); if(!g_pid) return FALSE;
    HANDLE hp=OpenProcess(PROCESS_ALL_ACCESS,FALSE,g_pid); if(!hp) return FALSE;
    SIZE_T sz=(wcslen(g_dllPath)+1)*sizeof(wchar_t);
    LPVOID mem=VirtualAllocEx(hp,NULL,sz,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!mem){CloseHandle(hp);return FALSE;}
    WriteProcessMemory(hp,mem,g_dllPath,sz,NULL);
    // Try Win32 QueueUserAPC too
    HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);
    if(snap!=INVALID_HANDLE_VALUE){
        THREADENTRY32 te={sizeof(te)};
        if(Thread32First(snap,&te))do{if(te.th32OwnerProcessID==g_pid){
            HANDLE ht=OpenThread(THREAD_ALL_ACCESS,FALSE,te.th32ThreadID);
            if(ht){QueueUserAPC((PAPCFUNC)LoadLibraryW,ht,(ULONG_PTR)mem);CloseHandle(ht);}
        }}while(Thread32Next(snap,&te));
        CloseHandle(snap);
    }
    HANDLE hT=CreateRemoteThread(hp,NULL,0,(LPTHREAD_START_ROUTINE)LoadLibraryW,mem,0,NULL);
    if(hT){WaitForSingleObject(hT,5000);CloseHandle(hT);CloseHandle(hp);return TRUE;}
    CloseHandle(hp); return FALSE;
}

// ─── Extract DLL from resource ───
static BOOL ExtractDll() {
    GetTempPathW(MAX_PATH,g_dllPath); wcscat(g_dllPath,L"Satella.dll");
    HRSRC hRes=FindResourceW(NULL,MAKEINTRESOURCEW(IDR_DLL1),RT_RCDATA);
    if(!hRes)return FALSE;
    HGLOBAL hGlob=LoadResource(NULL,hRes);
    if(!hGlob)return FALSE;
    LPVOID data=LockResource(hGlob);
    DWORD sz=SizeofResource(NULL,hRes);
    if(!data||sz<1000)return FALSE;
    HANDLE hFile=CreateFileW(g_dllPath,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(hFile==INVALID_HANDLE_VALUE)return FALSE;
    DWORD w;BOOL ok=WriteFile(hFile,data,sz,&w,NULL)&&w==sz;
    CloseHandle(hFile); return ok;
}

// ─── Hotkey window ───
static LRESULT CALLBACK WndProc(HWND hWnd,UINT msg,WPARAM w,LPARAM l){
    if(msg==WM_HOTKEY&&w==HOTKEY_ID){
        if(!FindPid())return 0;
        if(M1_CreateThread())return 0;
        if(M2_APC())return 0;
        if(M3_Hijack())return 0;
        M4_Win32();
        return 0;
    }
    if(msg==WM_DESTROY)PostQuitMessage(0);
    return DefWindowProcW(hWnd,msg,w,l);
}

int WINAPI WinMain(HINSTANCE hInst,HINSTANCE hPrev,char* lpCmd,int nCmdShow){
    InitSyscalls();
    if(!ExtractDll())return 0;
    WNDCLASSEXW wc={sizeof(wc),0,WndProc,0,0,hInst,NULL,NULL,NULL,NULL,L"SatInject",NULL};
    RegisterClassExW(&wc);
    HWND hWnd=CreateWindowExW(0,L"SatInject",NULL,WS_POPUP,0,0,0,0,NULL,NULL,hInst,NULL);
    if(!hWnd)return 0;
    RegisterHotKey(hWnd,HOTKEY_ID,MOD_NOREPEAT,VK_HOME);
    MSG msg;while(GetMessage(&msg,NULL,0,0)){TranslateMessage(&msg);DispatchMessage(&msg);}
    UnregisterHotKey(hWnd,HOTKEY_ID); return 0;
}
