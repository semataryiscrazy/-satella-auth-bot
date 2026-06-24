#include "Offsets.h"
#include "Utils.h"
#include "Scope.h"
#include "EntityCache.h"
#include <chrono>
#include <atomic>
static std::unordered_map<uintptr_t, EntityData> g_cache;
static WinSharedMutex g_cacheMutex;
static WinMutex g_cacheWriteMutex;
UnityMatrix cachedMatrix{};
int cachedScreenW = 0, cachedScreenH = 0;
uintptr_t cachedLocalPlayer = 0;
uint64_t lastEntityUpdate = 0;
std::atomic<uint64_t> renderMatrixTimestamp{0};

std::unordered_map<uintptr_t, EntityData>& GetEntityCache() { return g_cache; }
WinSharedMutex& GetCacheMutex() { return g_cacheMutex; }
WinMutex& GetCacheWriteMutex() { return g_cacheWriteMutex; }

static uintptr_t cachedGE = 0;
static uintptr_t cachedLocal = 0;
static auto engineTimer = std::chrono::steady_clock::now();

static uintptr_t GetEngine() {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - engineTimer).count() > 500) {
        engineTimer = now;
        cachedGE = 0;
        if (il2cpp < 0x10000) return 0;
        uintptr_t base = Ler<uintptr_t>(il2cpp + Offsets::InitBase);
        if (base != 0 && base > 0x10000) {
            uintptr_t facade = Ler<uintptr_t>(base);
            if (facade != 0 && facade > 0x10000) {
                uintptr_t sf = Ler<uintptr_t>(facade + Offsets::StaticClass);
                if (sf != 0 && sf > 0x10000) cachedGE = Ler<uintptr_t>(sf);
            }
        }
    }
    return cachedGE;
}

static uintptr_t GetLocal(uintptr_t ge) {
    if (ge == 0) return 0;
    // Verifica MatchStatus a cada chamada sem timer pra pegar transicao de round rapido
    uintptr_t m = Ler<uintptr_t>(ge + Offsets::CurrentMatch);
    if (m == 0) { cachedLocal = 0; return 0; }
    if (Ler<int>(m + Offsets::MatchStatus) != 1) { cachedLocal = 0; return 0; }
    // Se ja temos um local valido e o match continua, mantem
    if (cachedLocal != 0) return cachedLocal;
    // Soh tenta ler o local quando necessario
    cachedLocal = Ler<uintptr_t>(m + Offsets::LocalPlayer);
    return cachedLocal;
}

static std::atomic<bool> cacheRunning{ false };

static void CacheLoopImpl() {
    while (cacheRunning) {
        uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        if (now - lastEntityUpdate < 50) { WinSleepFor(5); continue; }

        uintptr_t ge = GetEngine(); if (ge == 0) { WinSleepFor(50); continue; }

        // Atualiza camera matrix
        uintptr_t ccm = Ler<uintptr_t>(ge + 0x74);
        if (ccm > 0x10000) {
            uintptr_t cam = Ler<uintptr_t>(ccm + string2Offset(AY_OBFUSCATE("0x10")));
            if (cam > 0x10000) {
                uintptr_t ic = Ler<uintptr_t>(cam + string2Offset(AY_OBFUSCATE("0x8")));
                if (ic > 0x10000) {
                    cachedMatrix = Ler<UnityMatrix>(ic + Offsets::ViewMatrix);
                    cachedScreenW = SWidth; cachedScreenH = SHeight;
                }
            }
        }

        cachedLocalPlayer = GetLocal(ge); if (cachedLocalPlayer == 0) { WinSleepFor(50); continue; }

        lastEntityUpdate = now;

        {
            WinWriteLockGuard lock(g_cacheMutex);
            for (auto it = g_cache.begin(); it != g_cache.end();)
                if (now - it->second.lastUpdate > 6000) it = g_cache.erase(it); else ++it;
        }

        uintptr_t dict = Ler<uintptr_t>(ge + Offsets::DictionaryEntities); if (dict == 0) continue;
        uintptr_t list = Ler<uintptr_t>(dict + Offsets::Il2CppDictionaryDataPtr); if (list == 0) continue;
        list += 0x10;
        int cnt = Ler<int>(dict + Offsets::Il2CppDictionaryCount); if (cnt < 1 || cnt > 200) continue;

        Vector3 myPos = Transform_ObterPosicao(Ler<uintptr_t>(cachedLocalPlayer + Offsets::MainTransform));

    for (int i = 0; i < cnt; i++) {
        uintptr_t e = Ler<uintptr_t>(list + (i * 0x10) + 0xC);
        if (e == 0 || e == cachedLocalPlayer) continue;

        EntityData fr;
        uintptr_t am = Ler<uintptr_t>(e + Offsets::AvatarManager); if (am == 0 || am < 0x10000) continue;
        uintptr_t uas = Ler<uintptr_t>(am + Offsets::UmaAvatarSimple); if (uas == 0 || uas < 0x10000) continue;
        uintptr_t ud = Ler<uintptr_t>(uas + Offsets::UMAData); if (ud == 0 || ud < 0x10000) continue;
        if (Ler<int>(e + string2Offset(AY_OBFUSCATE("0xC1C"))) == 0)
            if (!Ler<bool>(uas + Offsets::Avatar_IsVisible)) continue;
        uintptr_t pri = Ler<uintptr_t>(e + Offsets::PRIDataPool); if (pri == 0 || pri < 0x10000) continue;
        uintptr_t rdu = Ler<uintptr_t>(Ler<uintptr_t>(pri + Offsets::ReplicationDataPoolUnsafe) + Offsets::ReplicationDataUnsafe); if (rdu == 0 || rdu < 0x10000) continue;
        fr.health = Ler<short>(rdu + Offsets::Health); if (fr.health <= 0) continue;

        fr.address = e;
        fr.dying = false;
        fr.garota = Ler<bool>(e + Offsets::CDOBMFNCJHD);
        uintptr_t pd = Ler<uintptr_t>(e + Offsets::Player_Data);
        if (pd != 0 && pd > 0x10000) fr.dying = (Ler<int>(pd + Offsets::Player_IsDead) == 8);
        fr.isTeam = Ler<bool>(ud + Offsets::TeamMate);

        fr.headPos = GetHeadPosition(e); if (fr.headPos.X == 0 && fr.headPos.Y == 0 && fr.headPos.Z == 0) continue;
        fr.bodyPos = GetPlayerPosition(e, 0);
        fr.dist = Vector3::Distance(fr.bodyPos, myPos);

        Vector3 sb = World2Screen(cachedMatrix, fr.bodyPos);
        Vector3 sh = World2Screen(cachedMatrix, fr.headPos);
        if (sb.Z != 0 || sh.Z != 0) continue;
        fr.screenBody = sb; fr.screenHead = sh;

        auto bpi = Ler<uintptr_t>(e + Offsets::Player_Name);
        if (bpi != 0) {
            auto pn = Ler<uintptr_t>(bpi + string2Offset(AY_OBFUSCATE("0x18")));
            if (pn != 0) {
                int nc = Ler<int>(pn + string2Offset(AY_OBFUSCATE("0x8")));
                fr.name = ObterStr(pn + string2Offset(AY_OBFUSCATE("0xC")), nc);
            }
        }

        fr.hasBones = false;
        if (ESPEsqueleto) {
            static const uintptr_t maleBO[18] = {
                string2Offset(AY_OBFUSCATE("0x38")),string2Offset(AY_OBFUSCATE("0x14")),string2Offset(AY_OBFUSCATE("0x10")),string2Offset(AY_OBFUSCATE("0x48")),
                string2Offset(AY_OBFUSCATE("0x18")),string2Offset(AY_OBFUSCATE("0x1C")),string2Offset(AY_OBFUSCATE("0x20")),string2Offset(AY_OBFUSCATE("0x24")),
                string2Offset(AY_OBFUSCATE("0x28")),string2Offset(AY_OBFUSCATE("0x2C")),string2Offset(AY_OBFUSCATE("0x30")),string2Offset(AY_OBFUSCATE("0x34")),
                string2Offset(AY_OBFUSCATE("0x3C")),string2Offset(AY_OBFUSCATE("0x40")),string2Offset(AY_OBFUSCATE("0x44")),string2Offset(AY_OBFUSCATE("0x4C")),
                string2Offset(AY_OBFUSCATE("0x50")),string2Offset(AY_OBFUSCATE("0x54"))
            };
            static const uintptr_t femaleBO[18] = {
                string2Offset(AY_OBFUSCATE("0x3C")),string2Offset(AY_OBFUSCATE("0x18")),string2Offset(AY_OBFUSCATE("0x14")),string2Offset(AY_OBFUSCATE("0x10")),
                string2Offset(AY_OBFUSCATE("0x1C")),string2Offset(AY_OBFUSCATE("0x20")),string2Offset(AY_OBFUSCATE("0x24")),string2Offset(AY_OBFUSCATE("0x28")),
                string2Offset(AY_OBFUSCATE("0x2C")),string2Offset(AY_OBFUSCATE("0x30")),string2Offset(AY_OBFUSCATE("0x34")),string2Offset(AY_OBFUSCATE("0x38")),
                string2Offset(AY_OBFUSCATE("0x40")),string2Offset(AY_OBFUSCATE("0x44")),string2Offset(AY_OBFUSCATE("0x48")),string2Offset(AY_OBFUSCATE("0x4C")),
                string2Offset(AY_OBFUSCATE("0x50")),string2Offset(AY_OBFUSCATE("0x54"))
            };
            const uintptr_t* bo = fr.garota ? femaleBO : maleBO;
            fr.hasBones = true;
            for (int j = 0; j < 18; j++) {
                Vector3 wpos = ObterOssos(e, bo[j]);
                fr.boneWorld[j] = wpos;
                fr.bones[j] = World2Screen(cachedMatrix, wpos);
            }
        }

        fr.valid = true;
        fr.lastUpdate = now;
        { WinWriteLockGuard lock(g_cacheMutex); g_cache[e] = fr; }
    }
    }
}

static DWORD WINAPI SafeCacheLoop(LPVOID) {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll) {
        typedef NTSTATUS(NTAPI *NtSIT)(HANDLE,ULONG,PVOID,ULONG);
        NtSIT pNtSIT = (NtSIT)GetProcAddress(ntdll,"NtSetInformationThread");
        if (pNtSIT) pNtSIT(GetCurrentThread(),0x11,NULL,0);
    }
    __try { CacheLoopImpl(); } __except(EXCEPTION_EXECUTE_HANDLER) {}
    cacheRunning = false;
    return 0;
}

void UpdateEntityCache() {
    if (cacheRunning) return;
    cacheRunning = true;
    HANDLE h = CreateThread(NULL, 0, SafeCacheLoop, NULL, 0, NULL);
    if (h) CloseHandle(h);
}

void StopEntityCache() {
    cacheRunning = false;
}
