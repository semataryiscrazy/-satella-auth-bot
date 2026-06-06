#define _CRT_SECURE_NO_WARNINGS
#define IMGUI_DEFINE_MATH_OPERATORS
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <tchar.h>
#include <tlhelp32.h>
#include <winhttp.h>
#include <winternl.h>
#include <string.h>
#include <stdio.h>
#include "../source/Cfg/imgui/imgui.h"
#include "../source/Cfg/imgui/imgui_impl_dx11.h"
#include "../source/Cfg/imgui/imgui_impl_win32.h"
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "winhttp.lib")

static ID3D11Device* g_pd3dDevice = NULL;
static ID3D11DeviceContext* g_pd3dDeviceContext = NULL;
static IDXGISwapChain* g_pd3dSwapChain = NULL;
static ID3D11RenderTargetView* g_pd3dRenderTargetView = NULL;
static HWND g_hwnd = NULL;

static bool g_running = true;
static bool g_downloading = false;
static bool g_injecting = false;
static bool g_injected = false;
static float g_progress = 0.0f;
static char g_status[256] = "";
static char g_username[64] = "";
static char g_password[64] = "";
static bool g_loggedIn = false;
static int g_activeTab = 0;

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
static bool CreateDeviceD3D(HWND hWnd);
static void CleanupDeviceD3D();
static void RenderFrame();
static DWORD FindProcess(const wchar_t* name);
static DWORD FindThread(DWORD pid);

// ─── APC Injection ───
static BOOL InjectViaAPC(DWORD pid, const wchar_t* dllPath) {
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) return FALSE;

    size_t pathSize = (wcslen(dllPath) + 1) * sizeof(wchar_t);
    LPVOID pRemote = VirtualAllocEx(hProcess, NULL, pathSize, MEM_COMMIT, PAGE_READWRITE);
    if (!pRemote) { CloseHandle(hProcess); return FALSE; }
    if (!WriteProcessMemory(hProcess, pRemote, dllPath, pathSize, NULL)) {
        VirtualFreeEx(hProcess, pRemote, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return FALSE;
    }

    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    LPVOID pLoadLibraryW = GetProcAddress(hKernel32, "LoadLibraryW");

    // Try APC on alertable threads
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        VirtualFreeEx(hProcess, pRemote, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return FALSE;
    }

    THREADENTRY32 te = { sizeof(te) };
    BOOL found = FALSE;
    if (Thread32First(hSnapshot, &te)) do {
        if (te.th32OwnerProcessID == pid) {
            HANDLE hThread = OpenThread(THREAD_SET_CONTEXT, FALSE, te.th32ThreadID);
            if (hThread) {
                if (QueueUserAPC((PAPCFUNC)pLoadLibraryW, hThread, (ULONG_PTR)pRemote)) {
                    found = TRUE;
                    CloseHandle(hThread);
                    break;
                }
                CloseHandle(hThread);
            }
        }
    } while (Thread32Next(hSnapshot, &te));
    CloseHandle(hSnapshot);

    if (!found) {
        VirtualFreeEx(hProcess, pRemote, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return FALSE;
    }

    CloseHandle(hProcess);
    return TRUE;
}

// ─── Thread Hijacking Injection ───
typedef struct {
    BYTE shellcode[0x100];
    ULONGLONG pLdrLoadDll;
    wchar_t dllPath[256];
    UNICODE_STRING dllUs;
    ULONGLONG hModule;
    ULONGLONG origRIP;
    ULONGLONG origRSP;
    BYTE saveArea[0x200]; // For register saves
} REMOTE_DATA;

static BOOL InjectViaHijack(DWORD pid, const wchar_t* dllPath) {
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) return FALSE;

    // Allocate remote memory
    REMOTE_DATA* pRemote = (REMOTE_DATA*)VirtualAllocEx(hProcess, NULL, sizeof(REMOTE_DATA), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!pRemote) { CloseHandle(hProcess); return FALSE; }

    // Find ntdll!LdrLoadDll - same address in all x64 processes
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    ULONGLONG pLdrLoadDll = (ULONGLONG)GetProcAddress(hNtdll, "LdrLoadDll");
    if (!pLdrLoadDll) { VirtualFreeEx(hProcess, pRemote, 0, MEM_RELEASE); CloseHandle(hProcess); return FALSE; }

    // Build shellcode:
    // Save all volatile regs, align stack, call LdrLoadDll, restore, return to original code
    BYTE shellcode[] = {
        0x50,                                       // push rax
        0x51,                                       // push rcx
        0x52,                                       // push rdx
        0x53,                                       // push rbx
        0x54,                                       // push rsp (save current)
        0x55,                                       // push rbp
        0x56,                                       // push rsi
        0x57,                                       // push rdi
        0x41, 0x50,                                 // push r8
        0x41, 0x51,                                 // push r9
        0x41, 0x52,                                 // push r10
        0x41, 0x53,                                 // push r11
        0x41, 0x54,                                 // push r12
        0x41, 0x55,                                 // push r13
        0x41, 0x56,                                 // push r14
        0x41, 0x57,                                 // push r15
        0x9C,                                       // pushfq

        0x48, 0x83, 0xEC, 0x28,                     // sub rsp, 28h (shadow space + alignment)

        // lea rcx, [r13 + 0x100]  <- dllUs offset relative to REMOTE_DATA base
        // We'll use rip-relative addressing
        0x4C, 0x8D, 0x0D, 0x00, 0x00, 0x00, 0x00, // lea r9, [rip + offset_to_hModule]
        0x4C, 0x8D, 0x05, 0x00, 0x00, 0x00, 0x00, // lea r8, [rip + offset_to_dllUs]
        0x33, 0xD2,                                 // xor edx, edx (flags = 0)
        0x48, 0x8D, 0x0D, 0x00, 0x00, 0x00, 0x00, // lea rcx, [rip + offset_to_dllUs]

        // mov rax, [rip + pLdrLoadDll]
        0x48, 0x8B, 0x05, 0x00, 0x00, 0x00, 0x00, // mov rax, [rip + offset_to_pLdrLoadDll]
        0xFF, 0xD0,                                 // call rax

        0x48, 0x83, 0xC4, 0x28,                     // add rsp, 28h
        0x9D,                                       // popfq
        // Restore all regs
        0x41, 0x5F,                                 // pop r15
        0x41, 0x5E,                                 // pop r14
        0x41, 0x5D,                                 // pop r13
        0x41, 0x5C,                                 // pop r12
        0x41, 0x5B,                                 // pop r11
        0x41, 0x5A,                                 // pop r10
        0x41, 0x59,                                 // pop r9
        0x41, 0x58,                                 // pop r8
        0x5F,                                       // pop rdi
        0x5E,                                       // pop rsi
        0x5D,                                       // pop rbp
        0x5C,                                       // pop rsp -> restores original stack!
        0x5B,                                       // pop rbx
        0x5A,                                       // pop rdx
        0x59,                                       // pop rcx
        0x58,                                       // pop rax
        // Now we're back to original stack with original context
        // rip was pushed on stack before we started, we need to ret to it
        0xC3                                        // ret (pops saved RIP)
    };

    // Copy shellcode to remote
    SIZE_T written;
    if (!WriteProcessMemory(hProcess, pRemote, shellcode, sizeof(shellcode), &written)) {
        VirtualFreeEx(hProcess, pRemote, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return FALSE;
    }

    // Write pLdrLoadDll address
    if (!WriteProcessMemory(hProcess, &pRemote->pLdrLoadDll, &pLdrLoadDll, sizeof(pLdrLoadDll), &written)) {
        VirtualFreeEx(hProcess, pRemote, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return FALSE;
    }

    // Write DLL path
    if (!WriteProcessMemory(hProcess, pRemote->dllPath, (LPVOID)dllPath, (wcslen(dllPath) + 1) * sizeof(wchar_t), &written)) {
        VirtualFreeEx(hProcess, pRemote, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return FALSE;
    }

    // Fix shellcode offsets for rip-relative addressing
    // The shellcode at offset X uses rip-relative addressing
    // We need to calculate the correct offsets based on where things are in REMOTE_DATA

    // Actually, this gets complex. Let me use a simpler approach:
    // Write the UNICODE_STRING manually
    REMOTE_DATA localData = {0};
    localData.pLdrLoadDll = pLdrLoadDll;
    wcscpy(localData.dllPath, dllPath);
    localData.dllUs.Length = (USHORT)(wcslen(dllPath) * sizeof(wchar_t));
    localData.dllUs.MaximumLength = localData.dllUs.Length + sizeof(wchar_t);
    localData.dllUs.Buffer = &pRemote->dllPath[0];
    localData.origRIP = 0;
    localData.origRSP = 0;

    // Shellcode: uses absolute addresses for simplicity
    // Layout: every field is at a known offset from pRemote
    // [pRemote+0x000] shellcode
    // [pRemote+0x100] pLdrLoadDll
    // [pRemote+0x108] dllPath[256]
    // [pRemote+0x308] UNICODE_STRING
    // [pRemote+0x318] hModule
    // [pRemote+0x328] origRIP
    // [pRemote+0x330] origRSP

    BYTE sc2[] = {
        // Save context
        0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,   // push rax,rcx,rdx,rbx,rsp,rbp,rsi,rdi
        0x41, 0x50, 0x41, 0x51, 0x41, 0x52, 0x41, 0x53,   // push r8-r11
        0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57,   // push r12-r15
        0x9C,                                                // pushfq

        // sub rsp, 28h (shadow space)
        0x48, 0x83, 0xEC, 0x28,

        // rcx = &pRemote->dllUs (address: pRemote + 0x308)
        0x48, 0xB9, 0,0,0,0,0,0,0,0,                      // mov rcx, addr_of_dllUs
        // rdx = 0 (flags)
        0x33, 0xD2,
        // r8 = &pRemote->hModule (address: pRemote + 0x318)
        0x49, 0xB8, 0,0,0,0,0,0,0,0,                      // mov r8, addr_of_hModule
        // r9 = reserved (0)
        0x45, 0x33, 0xC9,
        // rax = pLdrLoadDll
        0x48, 0xB8, 0,0,0,0,0,0,0,0,                      // mov rax, pLdrLoadDll
        0xFF, 0xD0,                                        // call rax

        // add rsp, 28h
        0x48, 0x83, 0xC4, 0x28,
        // popfq
        0x9D,
        // restore registers (reverse order)
        0x41, 0x5F, 0x41, 0x5E, 0x41, 0x5D, 0x41, 0x5C,
        0x41, 0x5B, 0x41, 0x5A, 0x41, 0x59, 0x41, 0x58,
        0x5F, 0x5E, 0x5D, 0x5C, 0x5B, 0x5A, 0x59, 0x58,
        // ret (returns to original RIP that was pushed by caller)
        0xC3
    };

    // Set addresses in shellcode
    ULONGLONG baseAddr = (ULONGLONG)pRemote;
    *(ULONGLONG*)&sc2[28] = baseAddr + 0x308;  // rcx = &dllUs
    *(ULONGLONG*)&sc2[41] = baseAddr + 0x318;  // r8 = &hModule
    *(ULONGLONG*)&sc2[54] = pLdrLoadDll;        // rax = LdrLoadDll

    // Write shellcode
    if (!WriteProcessMemory(hProcess, pRemote, sc2, sizeof(sc2), &written)) {
        VirtualFreeEx(hProcess, pRemote, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return FALSE;
    }

    // Write DLL path
    if (!WriteProcessMemory(hProcess, &pRemote->dllPath, (LPVOID)dllPath, (wcslen(dllPath) + 1) * sizeof(wchar_t), &written)) {
        VirtualFreeEx(hProcess, pRemote, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return FALSE;
    }

    // Write UNICODE_STRING (Buffer pointer needs to point to remote dllPath)
    UNICODE_STRING us;
    us.Length = (USHORT)(wcslen(dllPath) * sizeof(wchar_t));
    us.MaximumLength = us.Length + sizeof(wchar_t);
    us.Buffer = &pRemote->dllPath[0];
    if (!WriteProcessMemory(hProcess, &pRemote->dllUs, &us, sizeof(us), &written)) {
        VirtualFreeEx(hProcess, pRemote, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return FALSE;
    }

    // Clear hModule
    ULONGLONG zero = 0;
    WriteProcessMemory(hProcess, &pRemote->hModule, &zero, sizeof(zero), &written);

    // Find a thread to hijack
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        VirtualFreeEx(hProcess, pRemote, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return FALSE;
    }

    THREADENTRY32 te = { sizeof(te) };
    BOOL hijacked = FALSE;
    if (Thread32First(hSnapshot, &te)) do {
        if (te.th32OwnerProcessID != pid) continue;
        HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, te.th32ThreadID);
        if (!hThread) continue;

        // Suspend thread
        if (SuspendThread(hThread) == (DWORD)-1) {
            CloseHandle(hThread);
            continue;
        }

        // Save original context
        CONTEXT ctx;
        ctx.ContextFlags = CONTEXT_FULL;
        if (!GetThreadContext(hThread, &ctx)) {
            ResumeThread(hThread);
            CloseHandle(hThread);
            continue;
        }

        // Save original RIP/RSP
        localData.origRIP = ctx.Rip;
        localData.origRSP = ctx.Rsp;
        WriteProcessMemory(hProcess, &pRemote->origRIP, &ctx.Rip, sizeof(ctx.Rip), &written);
        WriteProcessMemory(hProcess, &pRemote->origRSP, &ctx.Rsp, sizeof(ctx.Rsp), &written);

        // Set RIP to shellcode, push original RIP on stack for ret
        // New stack = original RSP - 8 (to push return address)
        ctx.Rsp -= 8;
        WriteProcessMemory(hProcess, (LPVOID)ctx.Rsp, &ctx.Rip, sizeof(ctx.Rip), &written);
        ctx.Rip = (ULONGLONG)pRemote;
        ctx.ContextFlags = CONTEXT_FULL;

        if (SetThreadContext(hThread, &ctx)) {
            ResumeThread(hThread);
            hijacked = TRUE;
            CloseHandle(hThread);
            break;
        }

        ResumeThread(hThread);
        CloseHandle(hThread);
    } while (Thread32Next(hSnapshot, &te));
    CloseHandle(hSnapshot);

    if (!hijacked) {
        VirtualFreeEx(hProcess, pRemote, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return FALSE;
    }

    // Wait a bit for DLL to load
    Sleep(2000);

    // Cleanup
    VirtualFreeEx(hProcess, pRemote, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    return TRUE;
}

// ═════════════════════════════════════════════
// CONFIGURAÇÃO SUPABASE — ALTERE AQUI!
// ═════════════════════════════════════════════
// 1. Crie uma conta grátis em https://supabase.com
// 2. Crie um projeto e copie os valores abaixo:
#define SUPABASE_HOST L"SEU_PROJETO.supabase.co"  // ex: "abc123.supabase.co"
#define SUPABASE_KEY  "sua-anon-key"              // Project API keys > anon public
#define STORAGE_BUCKET "satella-files"            // nome do bucket no Storage
// ═════════════════════════════════════════════

static BOOL SupabaseLogin(const char* email, const char* pass) {
    char body[512];
    snprintf(body, sizeof(body), "{\"email\":\"%s\",\"password\":\"%s\"}", email, pass);

    HINTERNET hSession = WinHttpOpen(L"SatellaLoader/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSession) return FALSE;
    HINTERNET hConnect = WinHttpConnect(hSession, SUPABASE_HOST, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return FALSE; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST",
        L"/auth/v1/token?grant_type=password", NULL, NULL, NULL, WINHTTP_FLAG_SECURE);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return FALSE; }

    char apikeyHdr[256]; snprintf(apikeyHdr, sizeof(apikeyHdr), "apikey: %s", SUPABASE_KEY);
    wchar_t wHdr[256]; MultiByteToWideChar(CP_UTF8, 0, apikeyHdr, -1, wHdr, 256);

    WinHttpAddRequestHeaders(hRequest, L"Content-Type: application/json", -1L, WINHTTP_ADDREQ_FLAG_ADD);
    WinHttpAddRequestHeaders(hRequest, wHdr, -1L, WINHTTP_ADDREQ_FLAG_ADD);

    BOOL ok = FALSE;
    if (WinHttpSendRequest(hRequest, NULL, 0, (LPVOID)body, (DWORD)strlen(body), (DWORD)strlen(body), 0) &&
        WinHttpReceiveResponse(hRequest, NULL)) {
        char buf[4096]; DWORD read = 0;
        std::string resp;
        while (WinHttpReadData(hRequest, buf, sizeof(buf)-1, &read) && read > 0) {
            buf[read] = 0; resp += buf;
        }
        if (resp.find("\"access_token\"") != std::string::npos) ok = TRUE;
    }
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return ok;
}

static BOOL DownloadFromSupabase(wchar_t* outPath) {
    HINTERNET hSession = WinHttpOpen(L"SatellaLoader/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSession) return FALSE;
    HINTERNET hConnect = WinHttpConnect(hSession, SUPABASE_HOST, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return FALSE; }

    char apikeyHdr[256]; snprintf(apikeyHdr, sizeof(apikeyHdr), "apikey: %s", SUPABASE_KEY);
    wchar_t wHdr[256]; MultiByteToWideChar(CP_UTF8, 0, apikeyHdr, -1, wHdr, 256);

    wchar_t listPath[256];
    swprintf(listPath, 256, L"/storage/v1/object/list/%hs", STORAGE_BUCKET);
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", listPath, NULL, NULL, NULL, WINHTTP_FLAG_SECURE);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return FALSE; }

    WinHttpAddRequestHeaders(hRequest, wHdr, -1L, WINHTTP_ADDREQ_FLAG_ADD);
    WinHttpAddRequestHeaders(hRequest, L"Content-Type: application/json", -1L, WINHTTP_ADDREQ_FLAG_ADD);
    const char* listBody = "{\"sortBy\":{\"column\":\"created_at\",\"order\":\"desc\"},\"limit\":1}";

    BOOL found = FALSE;
    if (WinHttpSendRequest(hRequest, NULL, 0, (LPVOID)listBody, (DWORD)strlen(listBody), (DWORD)strlen(listBody), 0) &&
        WinHttpReceiveResponse(hRequest, NULL)) {
        char buf[8192]; DWORD read = 0;
        std::string resp;
        while (WinHttpReadData(hRequest, buf, sizeof(buf)-1, &read) && read > 0) {
            buf[read] = 0; resp += buf;
        }
        auto npos = resp.find("\"name\":\"");
        if (npos != std::string::npos) {
            npos += 8;
            auto end = resp.find('"', npos);
            if (end != std::string::npos) {
                std::string fileName = resp.substr(npos, end - npos);
                wchar_t dlPath[512];
                swprintf(dlPath, 512, L"/storage/v1/object/public/%hs/%hs", STORAGE_BUCKET, fileName.c_str());
                WinHttpCloseHandle(hRequest);
                hRequest = WinHttpOpenRequest(hConnect, L"GET", dlPath, NULL, NULL, NULL, WINHTTP_FLAG_SECURE);
                if (hRequest) {
                    WinHttpAddRequestHeaders(hRequest, wHdr, -1L, WINHTTP_ADDREQ_FLAG_ADD);
                    if (WinHttpSendRequest(hRequest, NULL, 0, NULL, 0, 0, 0) &&
                        WinHttpReceiveResponse(hRequest, NULL)) {
                        HANDLE hFile = CreateFileW(outPath, GENERIC_WRITE, 0, NULL,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                        if (hFile != INVALID_HANDLE_VALUE) {
                            BYTE b[8192]; DWORD r; DWORD total = 0;
                            while (WinHttpReadData(hRequest, b, sizeof(b), &r) && r > 0) {
                                DWORD w; WriteFile(hFile, b, r, &w, NULL); total += r;
                            }
                            CloseHandle(hFile);
                            if (total > 1000) found = TRUE;
                        }
                    }
                }
            }
        }
    }
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return found;
}

// ─── Download + Inject Thread ───
static DWORD WINAPI DownloadThread(LPVOID) {
    g_downloading = true;
    g_progress = 0.0f;
    strcpy(g_status, "Baixando Satella.dll...");

    wchar_t dllPath[MAX_PATH];
    GetTempPathW(MAX_PATH, dllPath);
    wcscat(dllPath, L"Satella.dll");

    BOOL dllReady = FALSE;

    dllReady = DownloadFromSupabase(dllPath);
    if (!dllReady) {
        // Fallback: try hardcoded URLs
        struct { const wchar_t* host; const wchar_t* path; BOOL https; } urls[] = {
            { L"raw.githubusercontent.com", L"/semataryiscrazy/satella-bypass/main/Satella.dll", TRUE },
        };
        HINTERNET hSession = WinHttpOpen(L"SatellaLoader/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
        if (hSession) for (int i = 0; i < 1 && !dllReady; i++) {
            HINTERNET hConnect = WinHttpConnect(hSession, urls[i].host,
                urls[i].https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);
            if (!hConnect) continue;
            HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", urls[i].path,
                NULL, NULL, NULL, urls[i].https ? WINHTTP_FLAG_SECURE : 0);
            if (hRequest) {
                if (WinHttpSendRequest(hRequest, NULL, 0, NULL, 0, 0, 0) &&
                    WinHttpReceiveResponse(hRequest, NULL)) {
                    HANDLE hFile = CreateFileW(dllPath, GENERIC_WRITE, 0, NULL,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                    if (hFile != INVALID_HANDLE_VALUE) {
                        BYTE b[8192]; DWORD r; DWORD total = 0;
                        while (WinHttpReadData(hRequest, b, sizeof(b), &r) && r > 0) {
                            DWORD w; WriteFile(hFile, b, r, &w, NULL); total += r;
                        }
                        CloseHandle(hFile);
                        if (total > 1000) dllReady = TRUE;
                    }
                }
                WinHttpCloseHandle(hRequest);
            }
            WinHttpCloseHandle(hConnect);
        }
        WinHttpCloseHandle(hSession);
    }

    if (!dllReady) {
        g_downloading = false;
        strcpy(g_status, "Download falhou!");
        return 0;
    }

    g_progress = 0.5f;
    strcpy(g_status, "Procurando HD-Player.exe...");

    DWORD targetPid = FindProcess(L"HD-Player.exe");
    if (targetPid == 0) {
        g_downloading = false;
        strcpy(g_status, "HD-Player.exe nao encontrado!");
        return 0;
    }

    g_injecting = TRUE;

    // Try methods in order
    strcpy(g_status, "Tentando APC injection...");
    if (InjectViaAPC(targetPid, dllPath)) {
        g_injected = TRUE;
        strcpy(g_status, "Injetado via APC!");
        g_downloading = false;
        g_injecting = FALSE;
        return 0;
    }

    strcpy(g_status, "APC falhou. Tentando thread hijacking...");
    if (InjectViaHijack(targetPid, dllPath)) {
        g_injected = TRUE;
        strcpy(g_status, "Injetado via thread hijack!");
        g_downloading = false;
        g_injecting = FALSE;
        return 0;
    }

    strcpy(g_status, "Ambos metodos falharam!");
    g_downloading = false;
    g_injecting = FALSE;
    return 0;
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return TRUE;
    switch (msg) {
    case WM_SIZE:
        if (g_pd3dSwapChain && wParam != SIZE_MINIMIZED) {
            ImGui_ImplDX11_InvalidateDeviceObjects();
            if (g_pd3dRenderTargetView) { g_pd3dRenderTargetView->Release(); g_pd3dRenderTargetView = NULL; }
            ImGui_ImplDX11_CreateDeviceObjects();
        }
        return 0;
    case WM_DESTROY:
        g_running = false;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

static bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT res = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0,
        featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pd3dSwapChain,
        &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK) return false;

    ID3D11Texture2D* pBackBuffer;
    g_pd3dSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_pd3dRenderTargetView);
    pBackBuffer->Release();
    return true;
}

static void CleanupDeviceD3D() {
    if (g_pd3dRenderTargetView) { g_pd3dRenderTargetView->Release(); g_pd3dRenderTargetView = NULL; }
    if (g_pd3dSwapChain) { g_pd3dSwapChain->Release(); g_pd3dSwapChain = NULL; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}

static DWORD FindProcess(const wchar_t* name) {
    DWORD pid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe = { sizeof(pe) };
    if (Process32FirstW(snap, &pe)) do {
        if (_wcsicmp(pe.szExeFile, name) == 0) { pid = pe.th32ProcessID; break; }
    } while (Process32NextW(snap, &pe));
    CloseHandle(snap);
    return pid;
}

static void RenderFrame() {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImVec2 displaySz = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(displaySz);
    ImGui::Begin("Satella Loader", NULL,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetWindowPos();
    ImVec2 sz = ImGui::GetWindowSize();

    dl->AddRectFilled(pos, ImVec2(pos.x + sz.x, pos.y + sz.y), IM_COL32(12, 12, 20, 255));
    dl->AddRectFilledMultiColor(pos, ImVec2(pos.x + sz.x, pos.y + sz.y * 0.15f),
        IM_COL32(30, 20, 60, 60), IM_COL32(20, 30, 60, 30),
        IM_COL32(12, 12, 20, 0), IM_COL32(12, 12, 20, 0));
    dl->AddRectFilledMultiColor(pos, ImVec2(pos.x + sz.x, pos.y + 2),
        IM_COL32(80, 120, 255, 220), IM_COL32(140, 80, 200, 220),
        IM_COL32(200, 60, 150, 220), IM_COL32(80, 120, 255, 220));

    const char* title = "SATELLA LOADER";
    float titleSize = 26;
    ImVec2 ts = ImGui::CalcTextSize(title);
    float tx = pos.x + (sz.x - ts.x) * 0.5f;
    float ty = pos.y + 20;
    dl->AddText(ImGui::GetFont(), titleSize, ImVec2(tx + 1, ty + 1), IM_COL32(0, 0, 0, 100), title);
    dl->AddText(ImGui::GetFont(), titleSize, ImVec2(tx, ty), IM_COL32(255, 255, 255, 255), title);

    float contentY = pos.y + 70;

    if (!g_loggedIn) {
        float cx = (sz.x - 260) * 0.5f;
        ImGui::SetCursorPos(ImVec2(cx, contentY - pos.y));
        ImGui::BeginGroup();
        ImGui::TextColored(ImVec4(0.78f, 0.78f, 0.86f, 0.78f), "Username");
        ImGui::InputText("##user", g_username, sizeof(g_username));
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.78f, 0.78f, 0.86f, 0.78f), "Password");
        ImGui::InputText("##pass", g_password, sizeof(g_password), ImGuiInputTextFlags_Password);
        ImGui::Spacing();
        if (g_status[0]) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", g_status);
            ImGui::Spacing();
        }
        if (ImGui::Button("LOGIN", ImVec2(260, 36))) {
            if (strlen(g_username) > 0 && strlen(g_password) > 0) {
                strcpy(g_status, "Autenticando...");
                if (SupabaseLogin(g_username, g_password)) {
                    g_loggedIn = true;
                    g_activeTab = 1;
                    strcpy(g_status, "Bem-vindo!");
                } else {
                    strcpy(g_status, "Credenciais invalidas ou servidor offline");
                }
            } else {
                strcpy(g_status, "Preencha usuario e senha");
            }
        }
        ImGui::EndGroup();
    } else {
        ImGui::SetCursorPos(ImVec2(20, contentY - pos.y));
        ImGui::BeginGroup();
        ImVec4 tabActive = ImVec4(0.3f, 0.4f, 0.9f, 1.0f);
        ImVec4 tabInactive = ImVec4(0.15f, 0.15f, 0.2f, 1.0f);

        if (g_activeTab == 1) ImGui::PushStyleColor(ImGuiCol_Button, tabActive);
        else ImGui::PushStyleColor(ImGuiCol_Button, tabInactive);
        if (ImGui::Button("Home", ImVec2(120, 30))) g_activeTab = 1;
        ImGui::PopStyleColor();

        ImGui::SameLine();
        if (g_activeTab == 2) ImGui::PushStyleColor(ImGuiCol_Button, tabActive);
        else ImGui::PushStyleColor(ImGuiCol_Button, tabInactive);
        if (ImGui::Button("Settings", ImVec2(120, 30))) g_activeTab = 2;
        ImGui::PopStyleColor();
        ImGui::EndGroup();

        if (g_activeTab == 1) {
            float btnX = (sz.x - 200) * 0.5f;
            float btnY = pos.y + 130;

            ImGui::SetCursorPos(ImVec2(btnX, btnY - pos.y));

            if (g_injected) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.78f, 0.31f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                if (ImGui::Button("INJETADO!", ImVec2(200, 50))) {}
                ImGui::PopStyleColor(2);
            } else if (g_downloading || g_injecting) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.24f, 0.31f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.78f, 0.78f, 0.86f, 1.0f));
                if (ImGui::Button("CARREGANDO...", ImVec2(200, 50))) {}
                ImGui::PopStyleColor(2);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.26f, 0.53f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.42f, 0.34f, 0.61f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                if (ImGui::Button("INJECT", ImVec2(200, 50))) {
                    CreateThread(NULL, 0, DownloadThread, NULL, 0, NULL);
                }
                ImGui::PopStyleColor(3);
            }

            ImGui::SetCursorPos(ImVec2((sz.x - 260) * 0.5f, btnY - pos.y + 65));
            ImGui::ProgressBar(g_progress, ImVec2(260, 6), "");

            if (g_status[0]) {
                ImGui::SetCursorPos(ImVec2((sz.x - 260) * 0.5f, btnY - pos.y + 80));
                ImGui::TextColored(ImVec4(0.71f, 0.73f, 0.82f, 1.0f), "%s", g_status);
            }
        } else if (g_activeTab == 2) {
            ImGui::SetCursorPos(ImVec2(20, contentY - pos.y + 45));
            ImGui::BeginGroup();
            ImGui::TextColored(ImVec4(0.78f, 0.78f, 0.86f, 0.78f), "Informacoes:");
            ImGui::Text("Usuario: %s", g_username);
            ImGui::Text("Status: %s", g_injected ? "Injetado" : "Aguardando");
            ImGui::Spacing();
            if (ImGui::Button("Sair", ImVec2(120, 30))) {
                g_running = false;
            }
            ImGui::EndGroup();
        }
    }

    ImGui::End();
    ImGui::EndFrame();

    ImGui::Render();
    g_pd3dDeviceContext->OMSetRenderTargets(1, &g_pd3dRenderTargetView, NULL);
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    g_pd3dDeviceContext->ClearRenderTargetView(g_pd3dRenderTargetView, clearColor);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_pd3dSwapChain->Present(1, 0);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, hInstance,
                        NULL, NULL, NULL, NULL, L"SatellaLoader", NULL };
    RegisterClassExW(&wc);
    g_hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED, L"SatellaLoader",
        L"Satella Loader", WS_POPUP, 100, 100, 500, 400,
        NULL, NULL, wc.hInstance, NULL);
    SetLayeredWindowAttributes(g_hwnd, 0, 255, LWA_ALPHA);
    ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd);
    SetWindowPos(g_hwnd, HWND_TOPMOST,
        (GetSystemMetrics(SM_CXSCREEN) - 500) / 2,
        (GetSystemMetrics(SM_CYSCREEN) - 400) / 2,
        0, 0, SWP_NOSIZE);

    if (!CreateDeviceD3D(g_hwnd)) {
        CleanupDeviceD3D();
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    MSG msg;
    while (g_running) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) g_running = false;
        }
        RenderFrame();
        Sleep(10);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    DestroyWindow(g_hwnd);
    UnregisterClassW(L"SatellaLoader", hInstance);
    return 0;
}
