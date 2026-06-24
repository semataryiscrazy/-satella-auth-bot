#include "NoRecoil.hpp"
#include "Offsets.h"
#include "Utils.h"
#include "Scope.h"
#include <atomic>
#include <chrono>
#include <windows.h>

static std::atomic<bool> nr_running{ false };

static void NoRecoilLoop() {
    while (nr_running) {
        if (!NoRecoilEnabled || !cachedLocalPlayer) {
            WinSleepFor(10);
            continue;
        }

        uint32_t localPlayer = static_cast<uint32_t>(cachedLocalPlayer);
        if (localPlayer == 0) {
            WinSleepFor(10);
            continue;
        }

        uint32_t weaponAddr = Ler<uint32_t>(localPlayer + Offsets::Weapon);
        if (!weaponAddr) {
            WinSleepFor(10);
            continue;
        }

        uint32_t weaponData = Ler<uint32_t>(weaponAddr + Offsets::WeaponData);
        if (weaponData) {
            Escrever<float>(weaponData + Offsets::WeaponRecoil, 0.0f);
        }

        WinSleepFor(10);
    }
}

void Exploit::NoRecoil::Work() {
    if (!NoRecoilEnabled) {
        if (nr_running) Stop();
        return;
    }
    if (!nr_running) Start();
}

void Exploit::NoRecoil::Start() {
    if (nr_running) return;
    nr_running = true;
    CreateDetachedThread(NoRecoilLoop);
}

void Exploit::NoRecoil::Stop() {
    nr_running = false;
}
