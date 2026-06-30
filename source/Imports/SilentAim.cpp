#include "SilentAim.h"
#include <atomic>
#include <cmath>
#include "../Unity/Vector3.h"
#include "Offsets.h"
#include "Utils.h"
#include "Scope.h"
#include "Process.h"

namespace _0xW3X4Y5Z6 {

    static std::atomic<bool> running{ false };
    static std::atomic<uintptr_t> g_target{ 0 };

    static void RunImpl() {
        while (running) {
            if (!AimSilent) {
                WinSleepFor(50);
                continue;
            }

            bool noKey = (SilentAimKeyBind == 0);
            if (!noKey && (GetAsyncKeyState(SilentAimKeyBind) & 0x8000) == 0) {
                WinSleepFor(1);
                continue;
            }

            uintptr_t tg = g_target.load();
            if (tg == 0 || tg < 0x10000) {
                WinSleepFor(1);
                continue;
            }

            uint32_t weaponBase = Ler<uint32_t>(cachedLocalPlayer + Offsets::Sillent);
            if (weaponBase == 0 || weaponBase < 0x10000) {
                WinSleepFor(5);
                continue;
            }

            Vector3 targetPos;
            {
                WinSharedReadLockGuard lock(GetCacheMutex());
                auto& cache = GetEntityCache();
                auto it = cache.find((uintptr_t)tg);
                if (it == cache.end() || !it->second.valid)
                    continue;
                if (SilentAimHitbox == 0)
                    targetPos = it->second.headPos;
                else
                    targetPos = it->second.bodyPos;
            }

            for (int i = 0; i < 50 && running; i++) {
                if (!noKey && (GetAsyncKeyState(SilentAimKeyBind) & 0x8000) == 0) break;

                Vector3 startPos = Ler<Vector3>(weaponBase + Offsets::AimInfo_StartPos);
                if (startPos.X == 0 && startPos.Y == 0 && startPos.Z == 0) continue;

                Vector3 dir = targetPos - startPos;
                float mag = sqrtf(dir.X * dir.X + dir.Y * dir.Y + dir.Z * dir.Z);
                if (mag < 0.001f) continue;
                dir.X /= mag; dir.Y /= mag; dir.Z /= mag;

                Escrever<Vector3>(weaponBase + Offsets::AimInfo_RayDir, dir);
            }
        }
    }

    static DWORD WINAPI SafeRun(LPVOID) {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
        __try { RunImpl(); } __except(EXCEPTION_EXECUTE_HANDLER) {}
        running = false;
        return 0;
    }

    void Start() {
        if (running) return;
        running = true;
        HANDLE h = CreateThread(NULL, 0, SafeRun, NULL, 0, NULL);
        if (h) CloseHandle(h);
    }

    void Stop() {
        running = false;
    }

    void SetTarget(uintptr_t tg) { g_target.store(tg); }
}
