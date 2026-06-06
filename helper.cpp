#include <string>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <string.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Pega o caminho da DLL da linha de comando
    char dllPathA[260] = {0};
    if (lpCmdLine && lpCmdLine[0]) {
        strncpy(dllPathA, lpCmdLine, 259);
    } else {
        // fallback: C:\Users\s\AppData\Local\Discord\app-1.0.9238\Satella.dll
        strcpy(dllPathA, "C:\\Users\\s\\AppData\\Local\\Discord\\app-1.0.9238\\Satella.dll");
    }

    // Habilita debug privilege
    HANDLE hToken;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        TOKEN_PRIVILEGES tp;
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        LookupPrivilegeValueA(NULL, "SeDebugPrivilege", &tp.Privileges[0].Luid);
        AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
        CloseHandle(hToken);
    }

    // Encontra HD-Player
    DWORD pids[32]; int pc = 0;
    HANDLE ss = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (ss != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe = {sizeof(pe)};
        if (Process32FirstW(ss, &pe)) do {
            if (_wcsicmp(pe.szExeFile, L"HD-Player.exe") == 0 && pc < 32) pids[pc++] = pe.th32ProcessID;
        } while (Process32NextW(ss, &pe));
        CloseHandle(ss);
    }
    if (pc == 0) return 1;

    // Converte DLL path para wide
    wchar_t dllPathW[260];
    MultiByteToWideChar(CP_ACP, 0, dllPathA, -1, dllPathW, 260);

    // Injeta em cada processo
    int success = 0;
    for (int pi = 0; pi < pc; pi++) {
        HANDLE hp = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pids[pi]);
        if (!hp) continue;

        LPVOID m = VirtualAllocEx(hp, NULL, 260, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!m) { CloseHandle(hp); continue; }

        SIZE_T ws;
        WriteProcessMemory(hp, m, dllPathW, (wcslen(dllPathW) + 1) * 2, &ws);

        HANDLE ht = CreateRemoteThread(hp, NULL, 0, (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"), m, 0, NULL);
        if (ht) {
            CloseHandle(hp);
            WaitForSingleObject(ht, 10000);
            CloseHandle(ht);
            success++;
        } else {
            VirtualFreeEx(hp, m, 0, MEM_RELEASE);
            CloseHandle(hp);
        }
    }

    return success > 0 ? 0 : 2;
}
