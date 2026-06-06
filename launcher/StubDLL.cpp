#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>

#define SATELIA_PATH L"C:\\Users\\s\\AppData\\Local\\Temp\\Satella.dll"

static DWORD WINAPI SafeLoadThread(LPVOID) {
    Sleep(3000);
    __try {
        HMODULE h = LoadLibraryW(SATELIA_PATH);
        if (h) {
            // Keep trying to see if it stays loaded
            while (TRUE) {
                Sleep(1000);
                HMODULE check = NULL;
                if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, SATELIA_PATH, &check) || !check)
                    break;
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        // Satella crashed, but we caught it
    }
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hDll, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hDll);
        HANDLE hT = CreateThread(NULL, 0, SafeLoadThread, NULL, 0, NULL);
        if (hT) CloseHandle(hT);
    }
    return TRUE;
}
