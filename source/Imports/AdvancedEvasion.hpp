#pragma once
#include <windows.h>
#include <cstring>
#include <random>
#include <chrono>
#include <atomic>
#include <vector>
#include <unordered_map>

// Advanced Anti-Cheat Evasion Techniques
// Targets: BattlEye, EAC, Riot Vanguard, Kernel-mode AC systems

namespace AdvancedEvasion {

    // ─────────────────────────────────────────────────────────
    // HWID Spoofing (Hardware ID Masking)
    // ─────────────────────────────────────────────────────────

    class HWIDSpoofer {
    private:
        static constexpr size_t FAKE_SERIAL_LENGTH = 20;
        static std::string g_fakeHWID;
        static bool g_hwid_spoofed;

        static std::string GenerateFakeHWID() {
            std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
            std::string result;
            std::random_device rd;
            std::mt19937 gen(rd() ^ std::chrono::system_clock::now().time_since_epoch().count());
            std::uniform_int_distribution<> dis(0, chars.length() - 1);

            for (size_t i = 0; i < FAKE_SERIAL_LENGTH; i++) {
                result += chars[dis(gen)];
            }
            return result;
        }

    public:
        static void SpoofHWID() {
            if (g_hwid_spoofed) return;

            g_fakeHWID = GenerateFakeHWID();

            // Spoof registry disk serial
            HKEY hKey;
            if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, 
                L"HARDWARE\\DEVICEMAP\\Scsi\\Scsi Port 0\\Scsi Bus 0\\Target Id 0\\Logical Unit Id 0", 
                0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
                
                RegSetValueExA(hKey, "SerialNumber", 0, REG_SZ, 
                    (const BYTE*)g_fakeHWID.c_str(), g_fakeHWID.length() + 1);
                RegCloseKey(hKey);
            }

            // Spoof MAC address registry entries
            if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces",
                0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {

                wchar_t subKeyName[256];
                DWORD index = 0;

                while (RegEnumKeyW(hKey, index, subKeyName, sizeof(subKeyName) / sizeof(wchar_t)) == ERROR_SUCCESS) {
                    HKEY hSubKey;
                    if (RegOpenKeyExW(hKey, subKeyName, 0, KEY_WRITE, &hSubKey) == ERROR_SUCCESS) {
                        // Generate fake MAC
                        std::string fakeMac = GenerateFakeHWID().substr(0, 12);
                        RegSetValueExA(hSubKey, "DhcpPhysicalAddress", 0, REG_SZ,
                            (const BYTE*)fakeMac.c_str(), fakeMac.length() + 1);
                        RegCloseKey(hSubKey);
                    }
                    index++;
                }
                RegCloseKey(hKey);
            }

            g_hwid_spoofed = true;
        }

        static std::string GetFakeHWID() {
            if (!g_hwid_spoofed) SpoofHWID();
            return g_fakeHWID;
        }
    };

    std::string HWIDSpoofer::g_fakeHWID = "";
    bool HWIDSpoofer::g_hwid_spoofed = false;

    // ─────────────────────────────────────────────────────────
    // Behavioral Randomization (Anti-Pattern Detection)
    // ─────────────────────────────────────────────────────────

    class BehavioralRandomizer {
    private:
        static std::atomic<uint64_t> g_actionCounter;
        static std::mt19937_64 g_rng;
        static std::chrono::steady_clock::time_point g_lastTimingVariation;

    public:
        // Random timing variation (0-50ms jitter)
        static void ApplyTimingVariation() {
            std::uniform_int_distribution<> dis(0, 50);
            uint32_t jitter = dis(g_rng);
            WinSleepFor(jitter);
        }

        // Randomize operation sequence order
        static std::vector<int> GetRandomizedSequence(int count) {
            std::vector<int> sequence;
            for (int i = 0; i < count; i++) sequence.push_back(i);
            
            std::shuffle(sequence.begin(), sequence.end(), g_rng);
            return sequence;
        }

        // Add random delays between related operations (anti-linking)
        static void InjectRandomDelay(uint32_t minMs = 5, uint32_t maxMs = 50) {
            std::uniform_int_distribution<> dis(minMs, maxMs);
            WinSleepFor(dis(g_rng));
        }

        // Anti-pattern: Vary operation frequency
        static bool ShouldSkipOperation(float skipProbability = 0.05f) {
            std::uniform_real_distribution<> dis(0.0, 1.0);
            return dis(g_rng) < skipProbability;
        }

        // Track action frequency and vary it
        static uint64_t GetVariedActionCount() {
            uint64_t current = g_actionCounter.load();
            // Add random variance to make pattern less detectable
            std::uniform_int_distribution<> dis(-2, 2);
            return current + dis(g_rng);
        }

        static void IncrementActionCounter() {
            g_actionCounter++;
        }
    };

    std::atomic<uint64_t> BehavioralRandomizer::g_actionCounter{ 0 };
    std::mt19937_64 BehavioralRandomizer::g_rng(std::random_device{}() ^ 
        std::chrono::system_clock::now().time_since_epoch().count());

    // ─────────────────────────────────────────────────────────
    // Signature Spoofing (Anti-Scan Protection)
    // ─────────────────────────────────────────────────────────

    class SignatureSpoofer {
    private:
        struct SuspiciousPattern {
            const char* pattern;
            size_t patternLen;
            const char* replacement;
            size_t replacementLen;
        };

        static constexpr SuspiciousPattern PATTERNS[] = {
            // Aimbot signatures
            { "\x48\x89\xC1\x48\x89\xD8", 6, "\x90\x90\x90\x90\x90\x90", 6 },
            // ESP signatures
            { "\x0F\xB7\x45\x08\x66\x39", 6, "\x90\x90\x90\x90\x90\x90", 6 },
            // NoRecoil patterns
            { "\xFF\x15\x00\x00\x00\x00\x48", 7, "\x90\x90\x90\x90\x90\x90\x90", 7 },
        };

    public:
        static void ObfuscateMemorySignatures(HMODULE hModule) {
            if (!hModule) return;

            BYTE* base = (BYTE*)hModule;
            IMAGE_DOS_HEADER* idh = (IMAGE_DOS_HEADER*)base;
            
            if (idh->e_magic != IMAGE_DOS_SIGNATURE) return;

            IMAGE_NT_HEADERS64* nth = (IMAGE_NT_HEADERS64*)(base + idh->e_lfanew);
            if (nth->Signature != IMAGE_NT_SIGNATURE) return;

            IMAGE_SECTION_HEADER* sh = IMAGE_FIRST_SECTION(nth);
            DWORD oldProtect;

            // Scan and obfuscate suspicious code patterns
            for (WORD i = 0; i < nth->FileHeader.NumberOfSections; i++) {
                if (!(sh[i].Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;

                LPVOID sectionAddr = base + sh[i].VirtualAddress;
                SIZE_T sectionSize = sh[i].SizeOfRawData;

                if (!VirtualProtect(sectionAddr, sectionSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
                    continue;
                }

                BYTE* ptr = (BYTE*)sectionAddr;
                SIZE_T remaining = sectionSize;

                // Scan for suspicious patterns and obfuscate
                for (size_t pi = 0; pi < 3; pi++) {
                    const auto& pat = PATTERNS[pi];
                    
                    for (size_t i = 0; i < remaining; i++) {
                        if (memcmp(ptr + i, pat.pattern, pat.patternLen) == 0) {
                            // Replace with dead code (NOP sled with jitter)
                            for (size_t j = 0; j < pat.replacementLen; j++) {
                                ptr[i + j] = (j % 2 == 0) ? 0x90 : 0xCC;  // NOP/INT3 alternating
                            }
                            i += pat.replacementLen;
                        }
                    }
                }

                VirtualProtect(sectionAddr, sectionSize, oldProtect, &oldProtect);
                FlushInstructionCache(GetCurrentProcess(), sectionAddr, sectionSize);
            }
        }

        // Add harmless code bloat to confuse signature detection
        static void AddCodeBloat(HMODULE hModule) {
            std::mt19937 rng(std::random_device{}());
            std::uniform_int_distribution<> dis(0, 255);

            // Allocate temporary bloat memory
            size_t bloatSize = 4096;  // 4KB bloat
            BYTE* bloat = (BYTE*)malloc(bloatSize);
            
            if (bloat) {
                for (size_t i = 0; i < bloatSize; i++) {
                    bloat[i] = dis(rng);
                }
                // Intentionally never free - let OS handle it (harder to track allocation)
            }
        }
    };

    // ─────────────────────────────────────────────────────────
    // Timing Variation (Anti-Timing Analysis)
    // ─────────────────────────────────────────────────────────

    class TimingObfuscator {
    private:
        static std::atomic<uint64_t> g_lastOperationTime;
        static std::mt19937 g_rng;

    public:
        static void InjectRandomTimingVariation() {
            auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
            auto lastTime = g_lastOperationTime.load();
            
            // Detect if timing is suspiciously regular (pattern detected by AC)
            if (lastTime > 0) {
                uint64_t timeDiff = now - lastTime;
                
                // If timing is too regular (diff < 1ms), add random jitter
                if (timeDiff < 1000000) {  // 1ms in nanoseconds
                    std::uniform_int_distribution<> dis(1, 100);
                    WinSleepFor((DWORD)(dis(g_rng) / 10));
                }
            }

            g_lastOperationTime.store(now);
        }

        // Randomize execution path to break timing signatures
        static void ExecuteWithRandomDelay(std::function<void()> func) {
            std::uniform_int_distribution<> dis(0, 10);
            int randomPath = dis(g_rng);

            if (randomPath % 3 == 0) {
                WinThisThreadYield();
            } else if (randomPath % 3 == 1) {
                WinThisThreadYield();
            }

            func();

            if (randomPath % 2 == 0) {
                InjectRandomTimingVariation();
            }
        }
    };

    std::atomic<uint64_t> TimingObfuscator::g_lastOperationTime{ 0 };
    std::mt19937 TimingObfuscator::g_rng(std::random_device{}() ^ 
        std::chrono::system_clock::now().time_since_epoch().count());

    // ─────────────────────────────────────────────────────────
    // Stack Spoofing (Anti-Stack Trace Analysis)
    // ─────────────────────────────────────────────────────────

    class StackSpoofer {
    public:
        static void ObfuscateCallStack() {
            // Create dummy stack frames to confuse stack trace analysis
            volatile BYTE dummyBuffer[2048];
            
            // Fill with random data to create false stack traces
            for (size_t i = 0; i < sizeof(dummyBuffer); i++) {
                dummyBuffer[i] = (BYTE)(rand() % 256);
            }

            // Make compiler not optimize away the buffer
            volatile BYTE* ptr = dummyBuffer;
            for (int i = 0; i < 10; i++) {
                *ptr = rand();
            }
        }

        // Randomize function call order to confuse stack analysis
        static void RandomizeCallSequence() {
            std::mt19937 rng(std::random_device{}());
            std::uniform_int_distribution<> dis(0, 1000);

            // Execute dummy operations in random order
            if (dis(rng) % 2 == 0) {
                ObfuscateCallStack();
            } else {
                volatile int x = dis(rng);
                x = x * 2 + 1;
                x = x / 3;
            }
        }
    };

    // ─────────────────────────────────────────────────────────
    // Master Evasion Orchestrator
    // ─────────────────────────────────────────────────────────

    inline void InitializeAdvancedEvasion(HMODULE hModule) {
        // 1. Spoof HWID immediately
        HWIDSpoofer::SpoofHWID();

        // 2. Obfuscate code signatures
        SignatureSpoofer::ObfuscateMemorySignatures(hModule);

        // 3. Add code bloat
        SignatureSpoofer::AddCodeBloat(hModule);

        // 4. Initialize behavioral randomization
        BehavioralRandomizer::InjectRandomDelay(10, 50);

        // 5. Randomize stack
        StackSpoofer::RandomizeCallSequence();

        // 6. Timing obfuscation
        TimingObfuscator::InjectRandomTimingVariation();
    }

    // Called periodically to maintain evasion
    inline void MaintainAdvancedEvasion() {
        // Random behavioral variation
        if (BehavioralRandomizer::ShouldSkipOperation(0.02f)) {
            BehavioralRandomizer::ApplyTimingVariation();
        }

        // Timing variation
        TimingObfuscator::InjectRandomTimingVariation();

        // Stack randomization every 1000 operations
        if ((BehavioralRandomizer::GetVariedActionCount() % 1000) == 0) {
            StackSpoofer::RandomizeCallSequence();
        }

        BehavioralRandomizer::IncrementActionCounter();
    }
}
