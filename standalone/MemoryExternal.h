#pragma once
#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include <TlHelp32.h>
#include <memory>

class ExternalMemory {
public:
    static ExternalMemory& Get() {
        static ExternalMemory instance;
        return instance;
    }

    bool AttachToProcess(const std::wstring& processName) {
        detach();
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE) return false;
        PROCESSENTRY32W pe = { sizeof(PROCESSENTRY32W) };
        bool found = false;
        if (Process32FirstW(snapshot, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, processName.c_str()) == 0) {
                    m_pid = pe.th32ProcessID;
                    m_hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, m_pid);
                    if (m_hProcess) { found = true; m_processName = processName; break; }
                }
            } while (Process32NextW(snapshot, &pe));
        }
        CloseHandle(snapshot);
        if (!found) { m_pid = 0; m_hProcess = nullptr; }
        return found;
    }

    bool AttachToPid(DWORD pid) {
        detach();
        m_hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
        if (m_hProcess) { m_pid = pid; return true; }
        return false;
    }

    template<typename T>
    T Read(uintptr_t address) {
        T value{};
        if (m_hProcess) ReadProcessMemory(m_hProcess, (LPCVOID)address, &value, sizeof(T), nullptr);
        return value;
    }

    template<typename T>
    bool Write(uintptr_t address, T value) {
        if (!m_hProcess) return false;
        SIZE_T written = 0;
        return WriteProcessMemory(m_hProcess, (LPVOID)address, &value, sizeof(T), &written) && written == sizeof(T);
    }

    uintptr_t GetModuleBase(const std::wstring& moduleName) {
        if (!m_hProcess) return 0;
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32, m_pid);
        if (snapshot == INVALID_HANDLE_VALUE) return 0;
        MODULEENTRY32W me = { sizeof(MODULEENTRY32W) };
        uintptr_t base = 0;
        if (Module32FirstW(snapshot, &me)) {
            do {
                if (_wcsicmp(me.szModule, moduleName.c_str()) == 0 || _wcsicmp(me.szExePath, moduleName.c_str()) == 0) {
                    base = (uintptr_t)me.modBaseAddr; break;
                }
            } while (Module32NextW(snapshot, &me));
        }
        CloseHandle(snapshot);
        return base;
    }

    DWORD GetPid() const { return m_pid; }
    HANDLE GetHandle() const { return m_hProcess; }
    bool IsAttached() const { return m_hProcess != nullptr; }
    void detach() { if (m_hProcess) { CloseHandle(m_hProcess); m_hProcess = nullptr; } m_pid = 0; }
    ~ExternalMemory() { detach(); }
private:
    ExternalMemory() = default;
    ExternalMemory(const ExternalMemory&) = delete;
    ExternalMemory& operator=(const ExternalMemory&) = delete;
    HANDLE m_hProcess = nullptr;
    DWORD m_pid = 0;
    std::wstring m_processName;
};

#ifdef Ler
#undef Ler
#endif
#ifdef Escrever
#undef Escrever
#endif

template<typename T> inline T Ler(uintptr_t address) { return ExternalMemory::Get().Read<T>(address); }
template<typename T> inline void Escrever(uintptr_t address, T value) { ExternalMemory::Get().Write(address, value); }
