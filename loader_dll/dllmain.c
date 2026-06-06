#include <windows.h>
#include <stdio.h>

// Forward declaration from panel.c
extern void ShowPanel(void);

void WriteDebugLog(const char* msg);

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        {
            char logMsg[256];
            sprintf_s(logMsg, sizeof(logMsg), "DllMain: DLL_PROCESS_ATTACH, hModule=%p", hModule);
            WriteDebugLog(logMsg);
            
            // Inicializar o painel quando a DLL for carregada
            ShowPanel();
            
            WriteDebugLog("DllMain: ShowPanel() chamado");
        }
        break;
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        WriteDebugLog("DllMain: DLL_PROCESS_DETACH");
        break;
    }
    return TRUE;
}
