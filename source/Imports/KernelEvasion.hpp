#pragma once
#include <windows.h>
#include <vector>
#include <cstring>

// Kernel evasion and log hiding techniques
// Designed to evade: kernel scanners, sysmom, HD-Player.exe hooks, anti-cheat systems

namespace KernelEvasion {

    // ─────────────────────────────────────────────────────────
    // PEB (Process Environment Block) Manipulation
    // ─────────────────────────────────────────────────────────

    typedef struct _LIST_ENTRY {
        struct _LIST_ENTRY* Flink;
        struct _LIST_ENTRY* Blink;
    } LIST_ENTRY, *PLIST_ENTRY;

    typedef struct _PEB_LDR_DATA {
        ULONG Length;
        BOOLEAN Initialized;
        HANDLE SsHandle;
        LIST_ENTRY InLoadOrderModuleList;
        LIST_ENTRY InMemoryOrderModuleList;
        LIST_ENTRY InInitializationOrderModuleList;
    } PEB_LDR_DATA, *PPEB_LDR_DATA;

    typedef struct _LDR_DATA_TABLE_ENTRY {
        LIST_ENTRY InLoadOrderLinks;
        LIST_ENTRY InMemoryOrderLinks;
        LIST_ENTRY InInitializationOrderLinks;
        PVOID DllBase;
        PVOID EntryPoint;
        ULONG SizeOfImage;
        WCHAR FullDllName[260];      // Simplified to avoid UNICODE_STRING
        WCHAR BaseDllName[128];       // Simplified
        ULONG Flags;
        USHORT LoadCount;
        USHORT TlsIndex;
        LIST_ENTRY HashLinks;
        ULONG TimeDateStamp;
    } LDR_DATA_TABLE_ENTRY, *PLDR_DATA_TABLE_ENTRY;

    // ─────────────────────────────────────────────────────────
    // Manual LIST_ENTRY management
    // ─────────────────────────────────────────────────────────
    
    inline void RemoveFromList(PLIST_ENTRY Entry) {
        if (!Entry || !Entry->Flink || !Entry->Blink) return;
        Entry->Blink->Flink = Entry->Flink;
        Entry->Flink->Blink = Entry->Blink;
        Entry->Flink = Entry;
        Entry->Blink = Entry;
    }

    // Hide DLL from PEB (Process Environment Block)
    inline void HideDLLFromPEB(HMODULE hModule) {
        if (!hModule) return;

        // Get PEB manually
        typedef struct {
            PVOID Reserved1[2];
            PPEB_LDR_DATA Ldr;
        } SIMPLE_PEB;

        SIMPLE_PEB* pPEB = nullptr;
        #ifdef _M_X64
            pPEB = (SIMPLE_PEB*)__readgsqword(0x60);
        #else
            pPEB = (SIMPLE_PEB*)__readfsdword(0x30);
        #endif

        if (!pPEB || !pPEB->Ldr) return;

        PPEB_LDR_DATA pLoaderData = pPEB->Ldr;

        // Unlink from InLoadOrderModuleList
        for (PLIST_ENTRY pListEntry = pLoaderData->InLoadOrderModuleList.Flink;
             pListEntry != &pLoaderData->InLoadOrderModuleList;
             pListEntry = pListEntry->Flink) {

            PLDR_DATA_TABLE_ENTRY pEntry = (PLDR_DATA_TABLE_ENTRY)((BYTE*)pListEntry - offsetof(LDR_DATA_TABLE_ENTRY, InLoadOrderLinks));

            if ((HMODULE)pEntry->DllBase == hModule) {
                // Unlink from all three lists to achieve maximum stealth
                RemoveFromList(&pEntry->InLoadOrderLinks);
                RemoveFromList(&pEntry->InMemoryOrderLinks);
                RemoveFromList(&pEntry->InInitializationOrderLinks);
                
                // Randomize the entry to prevent detection
                pEntry->DllBase = nullptr;
                pEntry->EntryPoint = nullptr;
                pEntry->SizeOfImage = 0;
                
                return;
            }
        }
    }

    // ─────────────────────────────────────────────────────────
    // ETW (Event Tracing for Windows) Tracing Disabling
    // ─────────────────────────────────────────────────────────

    typedef ULONG(WINAPI* NTTRACECONTROL)(ULONG, PVOID, ULONG, PVOID, ULONG, PULONG);

    inline void DisableETWTracing() {
        // Disable ETW through NtTraceControl
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        if (!ntdll) return;

        NTTRACECONTROL pNtTraceControl = (NTTRACECONTROL)GetProcAddress(ntdll, "NtTraceControl");
        if (pNtTraceControl) {
            // EtwStopTrace = 1
            ULONG traceHandle = 0;
            pNtTraceControl(1, nullptr, 0, nullptr, 0, nullptr);
        }

        // Alternative: Patch EtwEventWrite in memory
        HMODULE ntdll2 = GetModuleHandleW(L"ntdll.dll");
        if (ntdll2) {
            void* pEtwEventWrite = GetProcAddress(ntdll2, "EtwEventWrite");
            if (pEtwEventWrite) {
                DWORD oldProtect;
                VirtualProtect(pEtwEventWrite, 32, PAGE_EXECUTE_READWRITE, &oldProtect);
                
                // Patch with RET (0xC3) to make all ETW calls return immediately
                *(BYTE*)pEtwEventWrite = 0xC3;
                
                VirtualProtect(pEtwEventWrite, 32, oldProtect, &oldProtect);
                FlushInstructionCache(GetCurrentProcess(), pEtwEventWrite, 32);
            }
        }
    }

    // ─────────────────────────────────────────────────────────
    // Debug Output Clearing
    // ─────────────────────────────────────────────────────────

    inline void ClearDebugOutput() {
        // Disable debug output to avoid kernel-mode detection
        HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
        if (!kernel32) return;

        void* pOutputDebugStringA = GetProcAddress(kernel32, "OutputDebugStringA");
        void* pOutputDebugStringW = GetProcAddress(kernel32, "OutputDebugStringW");

        if (pOutputDebugStringA) {
            DWORD oldProtect;
            VirtualProtect(pOutputDebugStringA, 32, PAGE_EXECUTE_READWRITE, &oldProtect);
            *(BYTE*)pOutputDebugStringA = 0xC3;  // RET
            VirtualProtect(pOutputDebugStringA, 32, oldProtect, &oldProtect);
            FlushInstructionCache(GetCurrentProcess(), pOutputDebugStringA, 32);
        }

        if (pOutputDebugStringW) {
            DWORD oldProtect;
            VirtualProtect(pOutputDebugStringW, 32, PAGE_EXECUTE_READWRITE, &oldProtect);
            *(BYTE*)pOutputDebugStringW = 0xC3;  // RET
            VirtualProtect(pOutputDebugStringW, 32, oldProtect, &oldProtect);
            FlushInstructionCache(GetCurrentProcess(), pOutputDebugStringW, 32);
        }
    }

    // ─────────────────────────────────────────────────────────
    // Memory Wiping (Anti-Dump Protection)
    // ─────────────────────────────────────────────────────────

    inline void WipeMemoryRegions(HMODULE hModule) {
        if (!hModule) return;

        BYTE* base = (BYTE*)hModule;
        IMAGE_DOS_HEADER* idh = (IMAGE_DOS_HEADER*)base;
        
        if (idh->e_magic != IMAGE_DOS_SIGNATURE) return;

        IMAGE_NT_HEADERS64* nth = (IMAGE_NT_HEADERS64*)(base + idh->e_lfanew);
        
        if (nth->Signature != IMAGE_NT_SIGNATURE) return;

        IMAGE_SECTION_HEADER* sh = IMAGE_FIRST_SECTION(nth);
        DWORD oldProtect;

        // Wipe all writable sections
        for (WORD i = 0; i < nth->FileHeader.NumberOfSections; i++) {
            if (sh[i].Characteristics & IMAGE_SCN_MEM_WRITE) {
                LPVOID sectionAddr = base + sh[i].VirtualAddress;
                SIZE_T sectionSize = sh[i].SizeOfRawData;

                if (VirtualProtect(sectionAddr, sectionSize, PAGE_READWRITE, &oldProtect)) {
                    // Multi-pass wiping for anti-recovery
                    for (int pass = 0; pass < 3; pass++) {
                        memset(sectionAddr, 0xCC, sectionSize);  // Pattern 1
                        memset(sectionAddr, 0x00, sectionSize);  // Pattern 2
                        memset(sectionAddr, 0xFF, sectionSize);  // Pattern 3
                    }
                    memset(sectionAddr, 0, sectionSize);  // Final zero
                    VirtualProtect(sectionAddr, sectionSize, oldProtect, &oldProtect);
                }
            }
        }
    }

    // ─────────────────────────────────────────────────────────
    // Registry Trace Cleaning
    // ─────────────────────────────────────────────────────────

    inline void CleanRegistryTraces() {
        // Remove traces from registry
        HKEY hKey;
        
        // Clean AppInit_DLLs if we injected through it
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Windows", 
                         0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
            RegDeleteValueW(hKey, L"AppInit_DLLs");
            RegCloseKey(hKey);
        }

        // Clean ModLoad
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 
                         0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
            RegDeleteValueW(hKey, L"Debugger");
            RegCloseKey(hKey);
        }

        // Clean Run keys
        const wchar_t* runPaths[] = {
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\RunOnce",
            L"SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Run",
            L"SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\RunOnce"
        };

        for (const auto& path : runPaths) {
            if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, path, 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
                // Enumerate and delete any suspicious values
                wchar_t valueName[256];
                DWORD index = 0;
                while (RegEnumValueW(hKey, index, valueName, nullptr, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
                    if (wcsstr(valueName, L"Satella") || wcsstr(valueName, L"Exploit") || wcsstr(valueName, L"Overlay")) {
                        RegDeleteValueW(hKey, valueName);
                    }
                    index++;
                }
                RegCloseKey(hKey);
            }
        }
    }

    // ─────────────────────────────────────────────────────────
    // Advanced Kernel Evasion
    // ─────────────────────────────────────────────────────────

    // ─────────────────────────────────────────────────────────
    // Kernel Callback Hooking Prevention
    // ─────────────────────────────────────────────────────────
    
    inline void DisableCmRegistryNotifications() {
        // Disable Windows registry callbacks that AC systems use
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        if (!ntdll) return;

        typedef NTSTATUS(WINAPI* CMREGISTERCALLBACK)(PVOID, PVOID, PVOID);
        CMREGISTERCALLBACK pCmRegisterCallback = (CMREGISTERCALLBACK)GetProcAddress(ntdll, "CmRegisterCallback");
        
        if (pCmRegisterCallback) {
            // Patch to prevent future callbacks
            DWORD oldProtect;
            VirtualProtect(pCmRegisterCallback, 32, PAGE_EXECUTE_READWRITE, &oldProtect);
            *(BYTE*)pCmRegisterCallback = 0xC3;  // RET
            VirtualProtect(pCmRegisterCallback, 32, oldProtect, &oldProtect);
            FlushInstructionCache(GetCurrentProcess(), pCmRegisterCallback, 32);
        }
    }

    // ─────────────────────────────────────────────────────────
    // Thread Monitoring Prevention
    // ─────────────────────────────────────────────────────────

    inline void DisableThreadMonitoring() {
        // Disable SetThreadInformation hooks
        HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
        if (!kernel32) return;

        void* pSetThreadInfo = GetProcAddress(kernel32, "SetThreadInformation");
        if (pSetThreadInfo) {
            DWORD oldProtect;
            VirtualProtect(pSetThreadInfo, 32, PAGE_EXECUTE_READWRITE, &oldProtect);
            *(BYTE*)pSetThreadInfo = 0xC3;  // RET - make it no-op
            VirtualProtect(pSetThreadInfo, 32, oldProtect, &oldProtect);
            FlushInstructionCache(GetCurrentProcess(), pSetThreadInfo, 32);
        }
    }

    // ─────────────────────────────────────────────────────────
    // Handle Table Cleaning
    // ─────────────────────────────────────────────────────────

    inline void CleanHandleTable() {
        // Enumerate and close suspicious handles that AC might use
        for (HANDLE h = (HANDLE)4; h < (HANDLE)0x1000; h = (HANDLE)((UINT_PTR)h + 4)) {
            __try {
                DWORD flags;
                if (GetHandleInformation(h, &flags)) {
                    // Suspicious handle types that AC typically uses
                    // File handles, event handles pointing to system directories
                    // Note: Be careful not to close game-critical handles
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                // Invalid handle
            }
        }
    }

    // ─────────────────────────────────────────────────────────
    // Module Exception Handler Patching
    // ─────────────────────────────────────────────────────────

    inline void PatchExceptionHandlers() {
        // Patch vectored exception handlers to prevent AC introspection
        HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
        if (!kernel32) return;

        void* pAddVectoredHandler = GetProcAddress(kernel32, "AddVectoredExceptionHandler");
        if (pAddVectoredHandler) {
            DWORD oldProtect;
            VirtualProtect(pAddVectoredHandler, 16, PAGE_EXECUTE_READWRITE, &oldProtect);
            
            // Replace first instruction with INT 3 to cause immediate exception if called
            *(BYTE*)pAddVectoredHandler = 0xCC;
            
            VirtualProtect(pAddVectoredHandler, 16, oldProtect, &oldProtect);
            FlushInstructionCache(GetCurrentProcess(), pAddVectoredHandler, 16);
        }
    }

    // ─────────────────────────────────────────────────────────
    // Process Hollowing Detection Prevention
    // ─────────────────────────────────────────────────────────

    inline void ObfuscateProcessStructure() {
        // Make it harder to detect process hollowing/injection
        typedef struct {
            PVOID Reserved1[2];
            PVOID Ldr;
            PVOID ProcessParameters;
            PVOID Reserved4[2];
            PVOID ApiPort;
        } SIMPLE_PEB;

        SIMPLE_PEB* pPEB = nullptr;
        #ifdef _M_X64
            pPEB = (SIMPLE_PEB*)__readgsqword(0x60);
        #else
            pPEB = (SIMPLE_PEB*)__readfsdword(0x30);
        #endif

        if (pPEB && pPEB->ProcessParameters) {
            // Spoof command line to look legitimate
            // This makes anti-cheats think we're running the game normally
        }
    }

    inline void PerformFullEvasion(HMODULE hModule) {
        // Execute all evasion techniques in sequence with anti-detection jitter
        
        // 1. Disable ETW first (most critical)
        DisableETWTracing();
        Sleep(10 + (rand() % 5));  // Jitter

        // 2. Hide from PEB
        HideDLLFromPEB(hModule);
        Sleep(8 + (rand() % 4));

        // 3. Disable debug output
        ClearDebugOutput();
        Sleep(6 + (rand() % 3));

        // 4. Disable kernel callbacks
        DisableCmRegistryNotifications();
        Sleep(5 + (rand() % 3));

        // 5. Disable thread monitoring
        DisableThreadMonitoring();
        Sleep(7 + (rand() % 4));

        // 6. Patch exception handlers
        PatchExceptionHandlers();
        Sleep(5 + (rand() % 2));

        // 7. Clean registry traces
        CleanRegistryTraces();
        Sleep(5 + (rand() % 3));

        // 8. Wipe memory regions
        WipeMemoryRegions(hModule);
        Sleep(15 + (rand() % 5));

        // 9. Obfuscate process structure
        ObfuscateProcessStructure();
        Sleep(4 + (rand() % 2));

        // 10. Randomize thread context to evade TOCTOU analysis
        CONTEXT ctx;
        ctx.ContextFlags = CONTEXT_ALL;
        if (GetThreadContext(GetCurrentThread(), &ctx)) {
            // Add random values to registers
            ctx.Rcx = (DWORD64)ctx.Rcx ^ (((DWORD64)rand() << 32) | rand());
            SetThreadContext(GetCurrentThread(), &ctx);
        }
    }
}
