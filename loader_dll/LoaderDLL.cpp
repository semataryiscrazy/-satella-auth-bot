#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "resource.h"

#define DLL_NAME L"Satella.dll"
#define HOOK_NAME L"SatellaHook.dll"
#define TARGET_EXE L"HD-Player.exe"

static BOOL ExtractRes(const wchar_t* outPath, int resId) {
    HRSRC hRes = FindResourceW(NULL, MAKEINTRESOURCEW(resId), RT_RCDATA);
    if (!hRes) return FALSE;
    HGLOBAL hGlob = LoadResource(NULL, hRes);
    if (!hGlob) return FALSE;
    LPVOID data = LockResource(hGlob);
    DWORD sz = SizeofResource(NULL, hRes);
    if (!data || sz < 100) return FALSE;
    HANDLE hFile = CreateFileW(outPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return FALSE;
    DWORD w; BOOL ok = WriteFile(hFile, data, sz, &w, NULL) && w == sz;
    CloseHandle(hFile);
    return ok;
}

static DWORD WINAPI SetupHook(LPVOID) {
    wchar_t temp[MAX_PATH];
    GetTempPathW(MAX_PATH, temp);

    wchar_t dllPath[MAX_PATH];
    wcscpy(dllPath, temp); wcscat(dllPath, DLL_NAME);
    wchar_t hookPath[MAX_PATH];
    wcscpy(hookPath, temp); wcscat(hookPath, HOOK_NAME);

    // Extract both DLLs
    if (!ExtractRes(dllPath, 1000)) return 0;
    if (!ExtractRes(hookPath, 1001)) return 0;

    // Install system-wide CBT hook — injects into all GUI processes
    HMODULE hHook = LoadLibraryW(hookPath);
    if (!hHook) return 0;

    HOOKPROC proc = (HOOKPROC)GetProcAddress(hHook, "HookProc");
    if (!proc) return 0;

    HHOOK hook = SetWindowsHookExW(WH_CBT, proc, hHook, 0);
    if (!hook) return 0;

    // Keep alive
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

// ─── Real version.dll forwarding ───
static HMODULE g_real = NULL;
static void LoadReal() {
    if (!g_real) {
        wchar_t p[MAX_PATH];
        GetSystemDirectoryW(p, MAX_PATH);
        wcscat(p, L"\\version.dll");
        g_real = LoadLibraryW(p);
    }
}

BOOL WINAPI DllMain(HINSTANCE hDll, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hDll);
        CreateThread(NULL, 0, SetupHook, NULL, 0, NULL);
    }
    return TRUE;
}

// Proxy exports
#pragma comment(linker, "/export:GetFileVersionInfoSizeW=forward_GetFileVersionInfoSizeW")
#pragma comment(linker, "/export:GetFileVersionInfoW=forward_GetFileVersionInfoW")
#pragma comment(linker, "/export:VerQueryValueW=forward_VerQueryValueW")
#pragma comment(linker, "/export:GetFileVersionInfoSizeA=forward_GetFileVersionInfoSizeA")
#pragma comment(linker, "/export:GetFileVersionInfoA=forward_GetFileVersionInfoA")
#pragma comment(linker, "/export:VerQueryValueA=forward_VerQueryValueA")
#pragma comment(linker, "/export:VerLanguageNameA=forward_VerLanguageNameA")
#pragma comment(linker, "/export:VerLanguageNameW=forward_VerLanguageNameW")

extern "C" {
    DWORD WINAPI forward_GetFileVersionInfoSizeW(LPCWSTR a, LPDWORD b) { LoadReal(); if(g_real){auto f=(decltype(&GetFileVersionInfoSizeW))GetProcAddress(g_real,"GetFileVersionInfoSizeW");if(f)return f(a,b);}return 0;}
    BOOL  WINAPI forward_GetFileVersionInfoW(LPCWSTR a, DWORD b, DWORD c, LPVOID d) { LoadReal(); if(g_real){auto f=(decltype(&GetFileVersionInfoW))GetProcAddress(g_real,"GetFileVersionInfoW");if(f)return f(a,b,c,d);}return 0;}
    BOOL  WINAPI forward_VerQueryValueW(LPCVOID a, LPCWSTR b, LPVOID* c, PUINT d) { LoadReal(); if(g_real){auto f=(decltype(&VerQueryValueW))GetProcAddress(g_real,"VerQueryValueW");if(f)return f(a,b,c,d);}return 0;}
    DWORD WINAPI forward_GetFileVersionInfoSizeA(LPCSTR a, LPDWORD b) { LoadReal(); if(g_real){auto f=(decltype(&GetFileVersionInfoSizeA))GetProcAddress(g_real,"GetFileVersionInfoSizeA");if(f)return f(a,b);}return 0;}
    BOOL  WINAPI forward_GetFileVersionInfoA(LPCSTR a, DWORD b, DWORD c, LPVOID d) { LoadReal(); if(g_real){auto f=(decltype(&GetFileVersionInfoA))GetProcAddress(g_real,"GetFileVersionInfoA");if(f)return f(a,b,c,d);}return 0;}
    BOOL  WINAPI forward_VerQueryValueA(LPCVOID a, LPCSTR b, LPVOID* c, PUINT d) { LoadReal(); if(g_real){auto f=(decltype(&VerQueryValueA))GetProcAddress(g_real,"VerQueryValueA");if(f)return f(a,b,c,d);}return 0;}
    DWORD WINAPI forward_VerLanguageNameA(DWORD a, LPSTR b, DWORD c) { LoadReal(); if(g_real){auto f=(decltype(&VerLanguageNameA))GetProcAddress(g_real,"VerLanguageNameA");if(f)return f(a,b,c);}return 0;}
    DWORD WINAPI forward_VerLanguageNameW(DWORD a, LPWSTR b, DWORD c) { LoadReal(); if(g_real){auto f=(decltype(&VerLanguageNameW))GetProcAddress(g_real,"VerLanguageNameW");if(f)return f(a,b,c);}return 0;}
}
