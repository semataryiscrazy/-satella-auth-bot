#include "WeaponAttributes.hpp"
#include "Offsets.h"
#include "Utils.h"
#include "Scope.h"
#include <random>
#include <chrono>

// Definição das estáticas
std::unordered_map<uint32_t, WeaponAttributes::WeaponOriginalValues> WeaponAttributes::restoredWeapons;
uint32_t WeaponAttributes::lastWeaponAddr = 0;
int WeaponAttributes::lastLevel = 0;
bool WeaponAttributes::wasEnabled = false;

// --- Anti-detecção: gerador e timer de jitter ---
static std::mt19937 g_WA_Rng{ std::random_device{}() };
static float RandomJitter(float maxPct) {
    std::uniform_real_distribution<float> dist(-maxPct, maxPct);
    return dist(g_WA_Rng);
}
static auto g_WA_LastJitterTime = std::chrono::steady_clock::now();
static constexpr int WA_JITTER_INTERVAL_MS = 480;

float WeaponAttributes::GetFireMultiplier(int level) {
    switch (level) {
    case 0: return 1.08f;
    case 1: return 1.16f;
    case 2: return 1.26f;
    case 3: return 1.35f;
    default: return 1.0f;
    }
}

void WeaponAttributes::Apply(uint32_t localPlayer, int level, bool enabled) {
    if (!localPlayer) return;

    if (!enabled) {
        if (wasEnabled) {
            RestoreAll();
            RestoreGlobalScales(localPlayer);
            wasEnabled = false;
            lastWeaponAddr = 0;
        }
        return;
    }

    wasEnabled = true;
    lastLevel = level;
    ApplyToWeapon(localPlayer, level);
    ApplyGlobalScales(localPlayer, level);
}

void WeaponAttributes::ApplyToWeapon(uint32_t localPlayer, int level) {
    uint32_t weapon = Ler<uint32_t>(localPlayer + Offsets::Weapon);
    if (!weapon) return;

    uint32_t weaponData = Ler<uint32_t>(weapon + 0x64);
    if (!weaponData) return;

    uint32_t weaponParams = Ler<uint32_t>(weaponData + Offsets::WeaponParams);
    if (!weaponParams) return;

    if (weaponParams == lastWeaponAddr && level == lastLevel) return;

    if (lastWeaponAddr != 0 && lastWeaponAddr != weaponParams) {
        auto it = restoredWeapons.find(lastWeaponAddr);
        if (it != restoredWeapons.end()) {
            Escrever<float>(lastWeaponAddr + Offsets::WeaponParams_FireInterval, it->second.fireInterval);
            Escrever<float>(lastWeaponAddr + Offsets::WeaponParams_RepeatFireInterval, it->second.repeatFireInterval);
            Escrever<float>(lastWeaponAddr + Offsets::WeaponParams_MultiFireInterval, it->second.multiFireInterval);
            Escrever<float>(weapon + Offsets::Weapon_AddFireSpeed, it->second.addFireSpeed);
        }
    }

    if (restoredWeapons.find(weaponParams) == restoredWeapons.end()) {
        WeaponOriginalValues orig;
        orig.fireInterval = Ler<float>(weaponParams + Offsets::WeaponParams_FireInterval);
        orig.repeatFireInterval = Ler<float>(weaponParams + Offsets::WeaponParams_RepeatFireInterval);
        orig.multiFireInterval = Ler<float>(weaponParams + Offsets::WeaponParams_MultiFireInterval);
        orig.addFireSpeed = Ler<float>(weapon + Offsets::Weapon_AddFireSpeed);
        restoredWeapons[weaponParams] = orig;
    }

    float mult = GetFireMultiplier(level);

    auto now = std::chrono::steady_clock::now();
    bool doJitter = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - g_WA_LastJitterTime).count() >= WA_JITTER_INTERVAL_MS;

    float jitter = doJitter ? RandomJitter(0.04f) : 0.0f;
    if (doJitter) g_WA_LastJitterTime = now;

    float effectiveMult = mult + (mult * jitter);

    float newFire   = restoredWeapons[weaponParams].fireInterval      * (1.0f / effectiveMult);
    float newRepeat = restoredWeapons[weaponParams].repeatFireInterval * (1.0f / effectiveMult);
    float newMulti  = restoredWeapons[weaponParams].multiFireInterval  * (1.0f / effectiveMult);
    float newAddSpeed = restoredWeapons[weaponParams].addFireSpeed + (0.08f * level);

    if (newFire   < 0.02f) newFire   = 0.02f;
    if (newRepeat < 0.02f) newRepeat = 0.02f;
    if (newMulti  < 0.02f) newMulti  = 0.02f;
    if (newAddSpeed < -0.5f) newAddSpeed = -0.5f;
    if (newAddSpeed >  1.0f) newAddSpeed =  1.0f;

    Escrever<float>(weaponParams + Offsets::WeaponParams_FireInterval,       newFire);
    Escrever<float>(weaponParams + Offsets::WeaponParams_RepeatFireInterval, newRepeat);
    Escrever<float>(weaponParams + Offsets::WeaponParams_MultiFireInterval,  newMulti);
    Escrever<float>(weapon       + Offsets::Weapon_AddFireSpeed,             newAddSpeed);

    lastWeaponAddr = weaponParams;
    lastLevel = level;
}

void WeaponAttributes::ApplyGlobalScales(uint32_t localPlayer, int level) {
    uint32_t playerAttr = Ler<uint32_t>(localPlayer + Offsets::PlayerAttributes);
    if (!playerAttr) return;

    float mult = GetFireMultiplier(level);
    float newScale = 1.0f / mult;
    if (newScale < 0.3f) newScale = 0.3f;
    if (newScale > 1.0f) newScale = 1.0f;

    static float lastWrittenScale = 1.0f;
    if (newScale != lastWrittenScale) {
        Escrever<float>(playerAttr + Offsets::PlayerAttributes_FireIntervalScale, newScale);
        lastWrittenScale = newScale;
    }
}

void WeaponAttributes::RestoreGlobalScales(uint32_t localPlayer) {
    uint32_t playerAttr = Ler<uint32_t>(localPlayer + Offsets::PlayerAttributes);
    if (playerAttr) {
        Escrever<float>(playerAttr + Offsets::PlayerAttributes_FireIntervalScale, 1.0f);
    }
}

void WeaponAttributes::RestoreAll() {
    for (auto& pair : restoredWeapons) {
        uint32_t weaponParams = pair.first;
        Escrever<float>(weaponParams + Offsets::WeaponParams_FireInterval, pair.second.fireInterval);
        Escrever<float>(weaponParams + Offsets::WeaponParams_RepeatFireInterval, pair.second.repeatFireInterval);
        Escrever<float>(weaponParams + Offsets::WeaponParams_MultiFireInterval, pair.second.multiFireInterval);
    }
    restoredWeapons.clear();
    lastWeaponAddr = 0;
}
