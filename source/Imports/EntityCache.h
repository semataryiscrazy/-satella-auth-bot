#pragma once
#include "../Unity/Vector3.h"
#include "Process.h"
#include <unordered_map>
#include <cstdint>

#pragma pack(push, 1)
struct EntityData {
    uintptr_t address = 0;
    Vector3 headPos{0,0,0}, bodyPos{0,0,0};
    Vector3 screenHead{0,0,0}, screenBody{0,0,0};
    Vector3 bones[18]{};
    Vector3 boneWorld[18]{};
    float dist = 0;
    short health = 0;
    bool valid = false, dying = false, isTeam = false;
    bool hasBones = false;
    bool garota = false;
    uint64_t lastUpdate = 0;
    std::string name;
    std::string weaponName;
};
#pragma pack(pop)

extern UnityMatrix cachedMatrix;
extern int cachedScreenW, cachedScreenH;
extern uintptr_t cachedLocalPlayer;

std::unordered_map<uintptr_t, EntityData>& GetEntityCache();
WinSharedMutex& GetCacheMutex();
