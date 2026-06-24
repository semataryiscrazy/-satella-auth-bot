#include "LockAim.h"
#include <atomic>
#include <chrono>
#include <windows.h>
#include "Offsets.h"
#include "Utils.h"
#include "Scope.h"

namespace LockAim {

    static std::atomic<bool> g_running{ false };
    static std::atomic<uintptr_t> g_target{ 0 };

    static void Disable(uintptr_t tg) {
        if (tg == 0) return;
        Escrever<uint32_t>((uint32_t)(tg + Offsets::ColliderINICDNFOFJB), 0u);
    }

    static void Loop() {
        static std::chrono::steady_clock::time_point kpt;
        static bool kp = false, dc = false;
        static uintptr_t ct = 0;

        while (g_running) {
            if (!Auth.Attached) {
                WinSleepFor(100);
                continue;
            }

            if (!AimbotLegit) {
                if (ct) { Disable(ct); ct = 0; }
                WinSleepFor(100);
                kp = false; dc = false;
                continue;
            }

            bool noKey = (AimbotKeyBind == 0);
            bool kh = noKey || (GetAsyncKeyState(AimbotKeyBind) & 0x8000) != 0;
            auto now = std::chrono::steady_clock::now();

            if (kh && !kp) { kpt = now; kp = true; dc = false; }
            else if (!kh && kp) {
                kp = false; dc = false;
                if (ct) { Disable(ct); ct = 0; }
                WinSleepFor(16);
                continue;
            }

            if (!kp) { WinSleepFor(16); continue; }

            // Delay configuravel
            if (!dc) {
                static const int delays[] = { 0, 235, 325, 415 };
                int ix = (AimbotPeitosIndex < 0) ? 0 : (AimbotPeitosIndex > 3) ? 3 : AimbotPeitosIndex;
                auto el = std::chrono::duration_cast<std::chrono::milliseconds>(now - kpt).count();
                if (el >= delays[ix]) dc = true;
                else { WinSleepFor(8); continue; }
            }

            uintptr_t tg = g_target.load();
            if (!tg) {
                if (ct) { Disable(ct); ct = 0; }
                WinSleepFor(16);
                continue;
            }

            if (ct && ct != tg) Disable(ct);
            ct = tg;

            uintptr_t hca = tg + Offsets::ColliderHECFNHJKOMN;
            uintptr_t laa = tg + Offsets::ColliderINICDNFOFJB;
            if (hca < 0x10000 || laa < 0x10000) {
                WinSleepFor(16);
                continue;
            }

            uint32_t hc = Ler<uint32_t>((uint32_t)hca);
            if (hc == 0) {
                WinSleepFor(16);
                continue;
            }

            uint32_t cr = Ler<uint32_t>((uint32_t)laa);
            if (cr != hc) {
                Escrever<uint32_t>((uint32_t)laa, 0u);
                Escrever<uint32_t>((uint32_t)laa, hc);
            }

            WinSleepFor(16);
        }
    }

    void Start() {
        if (g_running) return;
        g_running = true;
        CreateDetachedThread(Loop);
    }

    void Stop() { g_running = false; }

    void SetTarget(uintptr_t tg) { g_target.store(tg); }
}
