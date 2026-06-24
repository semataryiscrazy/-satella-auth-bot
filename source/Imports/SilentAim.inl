#include "SilentAim.h"
#include "EntityCache.h"
#include <cmath>
#include <algorithm>

AimSettings g_Aim;
static std::atomic<bool> g_AimRunning{ false };

static Vector3 GetHitboxPosition(const EntityData& e, int hitbox) {
    switch (hitbox) {
        case 0: return e.headPos;
        case 1: {
            float dx = e.headPos.X - e.bodyPos.X;
            float dy = e.headPos.Y - e.bodyPos.Y;
            float dz = e.headPos.Z - e.bodyPos.Z;
            return Vector3(e.headPos.X - dx * 0.15f, e.headPos.Y - dy * 0.15f, e.headPos.Z - dz * 0.15f);
        }
        case 2: return e.bodyPos;
        default: return e.headPos;
    }
}

static EntityData* FindClosestEnemy() {
    EntityData* best = nullptr;
    float bestDist = g_Aim.Fov;
    float cx = SWidth * 0.5f, cy = SHeight * 0.5f;

    WinSharedReadLockGuard lock(GetCacheMutex());
    for (auto& [addr, c] : GetEntityCache()) {
        if (!c.valid || c.isTeam || c.dying) continue;
        Vector3 hp = GetHitboxPosition(c, g_Aim.HitBox);
        Vector3 sp = World2Screen(cachedMatrix, hp);
        if (sp.Z != 0) continue;
        float dx = sp.X - cx, dy = sp.Y - cy;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist < bestDist) {
            bestDist = dist;
            best = &c;
        }
    }
    return best;
}

static void AimLoopImpl() {
    while (g_AimRunning) {
        if (!g_Aim.Enabled || !Auth.Attached || !cachedLocalPlayer) {
            WinSleepFor(10);
            continue;
        }
        bool keyHeld = (GetAsyncKeyState(g_Aim.KeyBind) & 0x8000) != 0;
        if (!keyHeld) {
            WinSleepFor(1);
            continue;
        }

        EntityData* target = FindClosestEnemy();
        if (!target) {
            WinSleepFor(1);
            continue;
        }

        uintptr_t headColliderAddr = target->address + Offsets::HeadCollider;
        uintptr_t lockedAimAddr = target->address + Offsets::ColliderINICDNFOFJB;

        uint32_t collider = Ler<uint32_t>(headColliderAddr);
        if (collider == 0) {
            WinSleepFor(1);
            continue;
        }
        Escrever<uint32_t>(lockedAimAddr, collider);
        WinSleepFor(g_Aim.UpdateMs);
        Escrever<uint32_t>(lockedAimAddr, 0);
    }
}

static DWORD WINAPI SafeAimLoop(LPVOID) {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll) {
        typedef NTSTATUS(NTAPI *NtSIT)(HANDLE,ULONG,PVOID,ULONG);
        NtSIT pNtSIT = (NtSIT)GetProcAddress(ntdll,"NtSetInformationThread");
        if (pNtSIT) pNtSIT(GetCurrentThread(),0x11,NULL,0);
    }
    __try { AimLoopImpl(); } __except(EXCEPTION_EXECUTE_HANDLER) {}
    g_AimRunning = false;
    return 0;
}

void StartSilentAim() {
    if (g_AimRunning) return;
    g_AimRunning = true;
    HANDLE h = CreateThread(NULL, 0, SafeAimLoop, NULL, 0, NULL);
    if (h) CloseHandle(h);
}

void StopSilentAim() {
    g_AimRunning = false;
}
