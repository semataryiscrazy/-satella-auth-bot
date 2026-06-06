#include <windows.h>
#include <process.h>

static HMODULE g_hRealVersion = NULL;

// Forward declaration from panel.c
extern void ShowPanel(void);

DWORD WINAPI PanelThreadProc(LPVOID lpParam) {
    ShowPanel();
    return 0;
}

void LoadRealVersion(void) {
    if (!g_hRealVersion) {
        wchar_t sysPath[MAX_PATH];
        GetSystemDirectoryW(sysPath, MAX_PATH);
        wcscat_s(sysPath, MAX_PATH, L"\\version.dll");
        g_hRealVersion = LoadLibraryW(sysPath);
    }
}

void InitializePanel(void) {
    static BOOL initialized = FALSE;
    if (!initialized) {
        initialized = TRUE;
        HANDLE hThread = CreateThread(NULL, 0, PanelThreadProc, NULL, 0, NULL);
        if (hThread) CloseHandle(hThread);
    }
}

DWORD WINAPI GetFileVersionInfoSizeW(LPCWSTR lptstrFilename, LPDWORD lpdwHandle) {
    InitializePanel();
    LoadRealVersion();
    if (g_hRealVersion) {
        typedef DWORD (WINAPI *PFN)(LPCWSTR, LPDWORD);
        PFN pFunc = (PFN)GetProcAddress(g_hRealVersion, "GetFileVersionInfoSizeW");
        if (pFunc) return pFunc(lptstrFilename, lpdwHandle);
    }
    return 0;
}

BOOL WINAPI GetFileVersionInfoW(LPCWSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData) {
    LoadRealVersion();
    if (g_hRealVersion) {
        typedef BOOL (WINAPI *PFN)(LPCWSTR, DWORD, DWORD, LPVOID);
        PFN pFunc = (PFN)GetProcAddress(g_hRealVersion, "GetFileVersionInfoW");
        if (pFunc) return pFunc(lptstrFilename, dwHandle, dwLen, lpData);
    }
    return FALSE;
}

BOOL WINAPI VerQueryValueW(LPCVOID pBlock, LPCWSTR lpSubBlock, LPVOID *lplpBuffer, PUINT puLen) {
    LoadRealVersion();
    if (g_hRealVersion) {
        typedef BOOL (WINAPI *PFN)(LPCVOID, LPCWSTR, LPVOID *, PUINT);
        PFN pFunc = (PFN)GetProcAddress(g_hRealVersion, "VerQueryValueW");
        if (pFunc) return pFunc(pBlock, lpSubBlock, lplpBuffer, puLen);
    }
    return FALSE;
}
