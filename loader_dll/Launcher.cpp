#include <windows.h>
#include <tlhelp32.h>
#include <string.h>

#pragma comment(lib, "kernel32.lib")

// Função para encontrar PID de um processo
DWORD FindProcessByName(const wchar_t* processName) {
    DWORD pid = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32W);
        
        if (Process32FirstW(hSnapshot, &pe32)) {
            do {
                if (wcscmp(pe32.szExeFile, processName) == 0) {
                    pid = pe32.th32ProcessID;
                    break;
                }
            } while (Process32NextW(hSnapshot, &pe32));
        }
        CloseHandle(hSnapshot);
    }
    return pid;
}

// Função para injetar DLL em um processo
BOOL InjectDLL(DWORD dwProcessId, const wchar_t* dllPath) {
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwProcessId);
    if (!hProcess) return FALSE;
    
    // Alocar memória no processo alvo
    size_t dllPathLen = (wcslen(dllPath) + 1) * sizeof(wchar_t);
    LPVOID pRemoteBuffer = VirtualAllocEx(hProcess, NULL, dllPathLen, MEM_COMMIT, PAGE_READWRITE);
    
    if (!pRemoteBuffer) {
        CloseHandle(hProcess);
        return FALSE;
    }
    
    // Escrever caminho da DLL na memória do processo
    if (!WriteProcessMemory(hProcess, pRemoteBuffer, (LPVOID)dllPath, dllPathLen, NULL)) {
        VirtualFreeEx(hProcess, pRemoteBuffer, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return FALSE;
    }
    
    // Obter endereço de LoadLibraryW
    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    LPVOID pLoadLibraryW = (LPVOID)GetProcAddress(hKernel32, "LoadLibraryW");
    
    // Criar thread remota para carregar a DLL
    HANDLE hRemoteThread = CreateRemoteThread(hProcess, NULL, 0, 
        (LPTHREAD_START_ROUTINE)pLoadLibraryW, pRemoteBuffer, 0, NULL);
    
    if (!hRemoteThread) {
        VirtualFreeEx(hProcess, pRemoteBuffer, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return FALSE;
    }
    
    // Aguardar conclusão
    WaitForSingleObject(hRemoteThread, INFINITE);
    
    // Limpar
    VirtualFreeEx(hProcess, pRemoteBuffer, 0, MEM_RELEASE);
    CloseHandle(hRemoteThread);
    CloseHandle(hProcess);
    
    return TRUE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Iniciar Discord
    STARTUPINFOW si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    
    if (!CreateProcessW(L"C:\\Users\\s\\AppData\\Local\\Discord\\app-1.0.9238\\Discord.exe", 
        NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        return 1;
    }
    
    // Aguardar Discord carregar (5 segundos)
    Sleep(5000);
    
    // Encontrar PID do Discord
    DWORD discordPid = FindProcessByName(L"Discord.exe");
    if (discordPid == 0) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return 1;
    }
    
    // Injetar nossa DLL no Discord
    wchar_t dllPath[MAX_PATH];
    GetModuleFileNameW(NULL, dllPath, MAX_PATH);
    
    // Substituir nome do executável pelo caminho da DLL
    wchar_t* lastBackslash = wcsrchr(dllPath, L'\\');
    if (lastBackslash) {
        wcscpy_s(lastBackslash + 1, MAX_PATH - (lastBackslash - dllPath + 1), L"LoaderDLL.dll");
    }
    
    InjectDLL(discordPid, dllPath);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    return 0;
}
