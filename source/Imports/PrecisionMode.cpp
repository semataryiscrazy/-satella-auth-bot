#include "PrecisionMode.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <vector>
#include "../Unity/Vector3.h"
#include "Offsets.h"
#include "Utils.h"
#include "Scope.h"
#include "EntityCache.h"

namespace _0xPrecision {

    static std::atomic<bool> running{ false };
    static HANDLE thread = nullptr;
    static bool wasActive = false;
    static uint32_t lastWd = 0;

    static bool ValidatePtr(uint32_t addr) {
        return addr > 0x10000 && addr < 0x7FFFFFFF;
    }

    static bool GetWeaponData(uint32_t& weaponData) {
        if (!Auth.Attached || !cachedLocalPlayer || !ValidatePtr(static_cast<uint32_t>(cachedLocalPlayer)))
            return false;
        uint32_t weapon = Ler<uint32_t>(static_cast<uint32_t>(cachedLocalPlayer) + Offsets::Weapon);
        if (!ValidatePtr(weapon)) return false;
        weaponData = Ler<uint32_t>(weapon + Offsets::WeaponData);
        return ValidatePtr(weaponData);
    }

    static void Run() {
        while (running) {
            try {
                bool enabled = PrecisionMode;
                uint32_t wd = 0;
                if (!GetWeaponData(wd)) {
                    wasActive = false;
                    WinSleepFor(100);
                    continue;
                }

                if (enabled && (!wasActive || wd != lastWd)) {
                    Escrever<float>(wd + Offsets::WD_Damage, 55.0f);
                    Escrever<float>(wd + Offsets::WD_FireInterval, 0.05f);
                    Escrever<float>(wd + 0x10, 0.0f);
                    Escrever<float>(wd + Offsets::WD_Range, 100.0f);
                } else if (!enabled && wasActive) {
                    if (wd == lastWd) {
                        Escrever<float>(wd + Offsets::WD_Damage, 15.0f);
                        Escrever<float>(wd + Offsets::WD_FireInterval, 0.12f);
                        Escrever<float>(wd + 0x10, 0.0f);
                        Escrever<float>(wd + Offsets::WD_Range, 10.0f);
                    }
                }

                wasActive = enabled;
                lastWd = wd;
                WinSleepFor(250);

            } catch (...) {
                continue;
            }
        }
    }

    void Start() {
        if (running) return;
        running = true;
        thread = CreateJoinableThread(Run);
    }

    void Stop() {
        if (!running) return;
        running = false;
        if (thread) { WaitForSingleObject(thread, INFINITE); CloseHandle(thread); thread = nullptr; }
    }
}
