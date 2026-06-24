#pragma once
#include <windows.h>
#include <cstring>
#include <unordered_map>
#include <vector>
#include <atomic>
#include <functional>
#include <random>

// Memory Integrity Verification System
// Detects runtime modifications of critical functions/data
// Prevents both external and internal tampering

namespace MemoryIntegrity {

    // CRC-32 for fast checksum calculation
    class CRC32 {
    private:
        static constexpr uint32_t POLYNOMIAL = 0xEDB88320;
        static uint32_t g_crcTable[256];
        static bool g_tableInitialized;

        static void InitializeTable() {
            if (g_tableInitialized) return;

            for (uint32_t i = 0; i < 256; i++) {
                uint32_t crc = i;
                for (uint8_t j = 0; j < 8; j++) {
                    if (crc & 1) {
                        crc = (crc >> 1) ^ POLYNOMIAL;
                    } else {
                        crc >>= 1;
                    }
                }
                g_crcTable[i] = crc;
            }
            g_tableInitialized = true;
        }

    public:
        static uint32_t Calculate(const void* data, size_t size) {
            InitializeTable();
            
            uint32_t crc = 0xFFFFFFFF;
            const uint8_t* buffer = (const uint8_t*)data;

            for (size_t i = 0; i < size; i++) {
                uint8_t byte = buffer[i];
                crc = (crc >> 8) ^ g_crcTable[(crc ^ byte) & 0xFF];
            }

            return crc ^ 0xFFFFFFFF;
        }
    };

    uint32_t CRC32::g_crcTable[256];
    bool CRC32::g_tableInitialized = false;

    // ─────────────────────────────────────────────────────────
    // Memory Checkpoint System
    // ─────────────────────────────────────────────────────────

    struct MemoryCheckpoint {
        uintptr_t address;
        size_t size;
        uint32_t checksum;
        const char* name;
        bool criticalFunction;
        uint32_t verificationFrequency;  // Check every N frames
    };

    class MemoryVerifier {
    private:
        static constexpr size_t MAX_CHECKPOINTS = 32;
        static MemoryCheckpoint g_checkpoints[MAX_CHECKPOINTS];
        static uint32_t g_checkpointCount;
        static std::atomic<uint64_t> g_verificationCounter;
        static std::atomic<bool> g_integrityCompromised;

        // Anti-tampering: Store checksums in separate location
        static std::unordered_map<uintptr_t, uint32_t> g_checksumCache;

    public:
        static void RegisterCheckpoint(uintptr_t address, size_t size, 
                                      const char* name = "Unknown", 
                                      bool isCritical = true, 
                                      uint32_t verifyFrequency = 10) {
            if (g_checkpointCount >= MAX_CHECKPOINTS) return;

            auto& cp = g_checkpoints[g_checkpointCount];
            cp.address = address;
            cp.size = size;
            cp.name = name;
            cp.criticalFunction = isCritical;
            cp.verificationFrequency = verifyFrequency;

            // Calculate and cache initial checksum
            cp.checksum = CRC32::Calculate((void*)address, size);
            g_checksumCache[address] = cp.checksum;

            g_checkpointCount++;
        }

        // Quick verification - fast path for critical functions
        static bool QuickVerify(uintptr_t address) {
            auto it = g_checksumCache.find(address);
            if (it == g_checksumCache.end()) return true;

            uint32_t currentChecksum = CRC32::Calculate((void*)address, 256);
            return currentChecksum == it->second;
        }

        // Full verification of all checkpoints
        static bool VerifyAll() {
            g_verificationCounter++;
            bool allValid = true;

            for (uint32_t i = 0; i < g_checkpointCount; i++) {
                const auto& cp = g_checkpoints[i];

                // Only verify on frequency intervals
                if (g_verificationCounter % cp.verificationFrequency != 0) {
                    continue;
                }

                uint32_t currentChecksum = CRC32::Calculate((void*)cp.address, cp.size);

                if (currentChecksum != cp.checksum) {
                    allValid = false;

                    if (cp.criticalFunction) {
                        // Critical function tampered - immediate action
                        g_integrityCompromised = true;
                        OnCriticalTampering(&cp);
                    } else {
                        // Non-critical tampering - log and monitor
                        OnMinorTampering(&cp);
                    }
                }
            }

            return allValid;
        }

        // Recalculate checksum for a region (after safe modifications)
        static void UpdateCheckpoint(uintptr_t address, size_t size) {
            for (uint32_t i = 0; i < g_checkpointCount; i++) {
                if (g_checkpoints[i].address == address && g_checkpoints[i].size == size) {
                    uint32_t newChecksum = CRC32::Calculate((void*)address, size);
                    g_checkpoints[i].checksum = newChecksum;
                    g_checksumCache[address] = newChecksum;
                    return;
                }
            }
        }

        // Get verification status
        static bool IsIntegrityCompromised() {
            return g_integrityCompromised.load();
        }

        // Tampering response handlers
    private:
        static void OnCriticalTampering(const MemoryCheckpoint* cp) {
            // Critical tampering detected - likely debugger or AC hook
            EmergencyCleanup();
        }

        static void OnMinorTampering(const MemoryCheckpoint* cp) {
            // Non-critical tampering detected
            volatile int dummy = 0;
            dummy++;
        }

        static void EmergencyCleanup() {
            // Clear sensitive data immediately
            if (!g_checksumCache.empty()) {
                volatile BYTE* ptr = (volatile BYTE*)&g_checksumCache;
                // Secure wipe - in practice, use SecureZeroMemory
                SecureZeroMemory((void*)ptr, sizeof(g_checksumCache));
            }
        }

    public:
        static uint64_t GetVerificationCount() {
            return g_verificationCounter.load();
        }
    };

    MemoryCheckpoint MemoryVerifier::g_checkpoints[MAX_CHECKPOINTS] = {};
    uint32_t MemoryVerifier::g_checkpointCount = 0;
    std::atomic<uint64_t> MemoryVerifier::g_verificationCounter{ 0 };
    std::atomic<bool> MemoryVerifier::g_integrityCompromised{ false };
    std::unordered_map<uintptr_t, uint32_t> MemoryVerifier::g_checksumCache;

    // ─────────────────────────────────────────────────────────
    // Runtime Consistency Checker
    // ─────────────────────────────────────────────────────────

    struct ConsistencyRecord {
        uint64_t timestamp;
        uintptr_t address;
        uint32_t expectedValue;
        uint32_t actualValue;
    };

    class ConsistencyChecker {
    private:
        static std::vector<ConsistencyRecord> g_inconsistencies;
        static std::atomic<uint32_t> g_inconsistencyCount;

    public:
        // Verify that values match expected state
        static bool VerifyMemoryState(uintptr_t address, uint32_t expectedValue, size_t size = 4) {
            uint32_t actualValue = 0;
            
            __try {
                if (IsBadReadPtr((void*)address, size)) {
                    return false;
                }

                memcpy_s(&actualValue, sizeof(actualValue), (void*)address, size);

                if (actualValue != expectedValue) {
                    RecordInconsistency(address, expectedValue, actualValue);
                    return false;
                }

                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
        }

        // Track memory inconsistencies for analysis
        static void RecordInconsistency(uintptr_t address, uint32_t expected, uint32_t actual) {
            ConsistencyRecord record;
            record.timestamp = GetTickCount64();
            record.address = address;
            record.expectedValue = expected;
            record.actualValue = actual;

            g_inconsistencies.push_back(record);
            g_inconsistencyCount++;

            // Keep history limited to last 100 records
            if (g_inconsistencies.size() > 100) {
                g_inconsistencies.erase(g_inconsistencies.begin());
            }
        }

        static uint32_t GetInconsistencyCount() {
            return g_inconsistencyCount.load();
        }

        // Check if we're being debugged based on memory patterns
        static bool DetectDebuggerByMemoryPatterns() {
            // Check for common debugger signatures
            HMODULE dbgHelp = GetModuleHandleW(L"dbghelp.dll");
            HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");

            // If both loaded together in suspicious way, likely debugger
            if (dbgHelp && ntdll) {
                return true;
            }

            return false;
        }
    };

    std::vector<ConsistencyRecord> ConsistencyChecker::g_inconsistencies;
    std::atomic<uint32_t> ConsistencyChecker::g_inconsistencyCount{ 0 };

    // ─────────────────────────────────────────────────────────
    // Function Guard (Trap Tampered Functions)
    // ─────────────────────────────────────────────────────────

    class FunctionGuard {
    private:
        static constexpr size_t GUARD_SIZE = 16;  // First 16 bytes of function
        static std::unordered_map<uintptr_t, std::vector<uint8_t>> g_functionSnapshots;

    public:
        static void TrapFunction(uintptr_t funcAddress, const char* funcName) {
            // Take snapshot of function prologue
            std::vector<uint8_t> snapshot(GUARD_SIZE);
            
            if (IsBadReadPtr((void*)funcAddress, GUARD_SIZE)) {
                return;
            }

            memcpy(snapshot.data(), (void*)funcAddress, GUARD_SIZE);
            g_functionSnapshots[funcAddress] = snapshot;
        }

        // Check if function has been modified (hooked)
        static bool IsFunctionHooked(uintptr_t funcAddress) {
            auto it = g_functionSnapshots.find(funcAddress);
            if (it == g_functionSnapshots.end()) return false;

            if (IsBadReadPtr((void*)funcAddress, GUARD_SIZE)) {
                return true;  // Bad read = likely hooked
            }

            uint8_t currentSnapshot[GUARD_SIZE];
            memcpy(currentSnapshot, (void*)funcAddress, GUARD_SIZE);

            // Compare first few bytes
            return memcmp(currentSnapshot, it->second.data(), GUARD_SIZE) != 0;
        }

        // Get all trapped functions for verification
        static size_t VerifyAllTraps() {
            size_t hookedCount = 0;

            for (auto& pair : g_functionSnapshots) {
                if (IsFunctionHooked(pair.first)) {
                    hookedCount++;
                }
            }

            return hookedCount;
        }
    };

    std::unordered_map<uintptr_t, std::vector<uint8_t>> FunctionGuard::g_functionSnapshots;

    // ─────────────────────────────────────────────────────────
    // Master Integrity System Initialization
    // ─────────────────────────────────────────────────────────

    inline void InitializeIntegritySystem() {
        // Register critical functions for monitoring
        // Note: These addresses are placeholders - adjust for your DLL
        // Uncomment when you know exact function addresses
        
        // MemoryVerifier::RegisterCheckpoint(
        //     (uintptr_t)&SomeFunction,
        //     256,
        //     "FunctionName",
        //     true,
        //     5
        // );
    }

    inline void VerifyIntegrity() {
        // Call periodically (every render frame)
        if (MemoryVerifier::VerifyAll()) {
            // All checksums valid
        } else {
            // Tampering detected - logged in verification
        }

        // Check for function hooks
        size_t hookedFunctions = FunctionGuard::VerifyAllTraps();
        if (hookedFunctions > 0) {
            // Functions have been hooked - possible debugger/AC
        }
    }

    // Advanced detection: Check for anti-debug traps
    inline bool IsBeingDebugged() {
        // Method 1: Check for debugger via WinAPI
        BOOL isDebugged = FALSE;
        if (CheckRemoteDebuggerPresent(GetCurrentProcess(), &isDebugged) && isDebugged) {
            return true;
        }

        // Method 2: Check for memory patterns
        if (ConsistencyChecker::DetectDebuggerByMemoryPatterns()) {
            return true;
        }

        // Method 3: Check for function hooks
        if (FunctionGuard::VerifyAllTraps() > 5) {
            return true;
        }

        return false;
    }
}
