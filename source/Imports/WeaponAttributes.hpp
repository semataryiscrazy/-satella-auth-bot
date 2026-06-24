#pragma once
#include <cstdint>
#include <unordered_map>

class WeaponAttributes {
public:
    // Chamar uma vez por frame (com localPlayer v�lido)
    static void Apply(uint32_t localPlayer, int level, bool enabled);

    // Restaura todos os valores (chamado quando encerra ou desativa)
    static void RestoreAll();
    static void RestoreGlobalScales(uint32_t localPlayer);

private:
    struct WeaponOriginalValues {
        float fireInterval;
        float repeatFireInterval;
        float multiFireInterval;
        float addFireSpeed;      // salvo por endere�o da arma (n�o do params)
    };

    // Mapa: endere�o da arma -> valores originais
    static std::unordered_map<uint32_t, WeaponOriginalValues> restoredWeapons;
    static uint32_t lastWeaponAddr;
    static int lastLevel;
    static bool wasEnabled;

    static float GetFireMultiplier(int level);
    static void ApplyToWeapon(uint32_t localPlayer, int level);
    static void ApplyGlobalScales(uint32_t localPlayer, int level);
};
