#pragma once

#include <vector>
#include <unordered_map>
#include <atomic>
#include <windows.h>

namespace ImGui {} // ensure namespace exists before using directive
using namespace ImGui;

class Discord;
inline Discord* DiscordRPC;

// ─── Win32 threading primitives (replaces <mutex>, <thread>, <shared_mutex>) ───

struct WinMutex {
    CRITICAL_SECTION cs;
    WinMutex() { InitializeCriticalSection(&cs); }
    ~WinMutex() { DeleteCriticalSection(&cs); }
    WinMutex(const WinMutex&) = delete;
    WinMutex& operator=(const WinMutex&) = delete;
};

struct WinSharedMutex {
    SRWLOCK srw;
    WinSharedMutex() { InitializeSRWLock(&srw); }
    WinSharedMutex(const WinSharedMutex&) = delete;
    WinSharedMutex& operator=(const WinSharedMutex&) = delete;
};

struct WinLockGuard {
    WinMutex& mtx;
    WinLockGuard(WinMutex& m) : mtx(m) { EnterCriticalSection(&mtx.cs); }
    ~WinLockGuard() { LeaveCriticalSection(&mtx.cs); }
    WinLockGuard(const WinLockGuard&) = delete;
    WinLockGuard& operator=(const WinLockGuard&) = delete;
};

struct WinWriteLockGuard {
    WinSharedMutex& mtx;
    WinWriteLockGuard(WinSharedMutex& m) : mtx(m) { AcquireSRWLockExclusive(&mtx.srw); }
    ~WinWriteLockGuard() { ReleaseSRWLockExclusive(&mtx.srw); }
    WinWriteLockGuard(const WinWriteLockGuard&) = delete;
    WinWriteLockGuard& operator=(const WinWriteLockGuard&) = delete;
};

struct WinSharedReadLockGuard {
    WinSharedMutex& mtx;
    WinSharedReadLockGuard(WinSharedMutex& m) : mtx(m) { AcquireSRWLockShared(&mtx.srw); }
    ~WinSharedReadLockGuard() { ReleaseSRWLockShared(&mtx.srw); }
    WinSharedReadLockGuard(const WinSharedReadLockGuard&) = delete;
    WinSharedReadLockGuard& operator=(const WinSharedReadLockGuard&) = delete;
};

template<typename F>
struct WinThreadData {
    F f;
    WinThreadData(F&& f_) : f(std::move(f_)) {}
    static DWORD WINAPI Proc(LPVOID p) {
        WinThreadData* d = static_cast<WinThreadData*>(p);
        d->f();
        delete d;
        return 0;
    }
};

template<typename F>
HANDLE CreateJoinableThread(F f) {
    WinThreadData<F>* d = new WinThreadData<F>(std::move(f));
    HANDLE h = CreateThread(nullptr, 0, &WinThreadData<F>::Proc, d, 0, nullptr);
    if (!h) { delete d; return nullptr; }
    return h;
}

template<typename F>
void CreateDetachedThread(F f) {
    WinThreadData<F>* d = new WinThreadData<F>(std::move(f));
    HANDLE h = CreateThread(nullptr, 0, &WinThreadData<F>::Proc, d, 0, nullptr);
    if (h) CloseHandle(h);
    else delete d;
}

inline void WinSleepFor(DWORD ms) { Sleep(ms); }
inline void WinThisThreadYield() { Sleep(0); }

// ─── End Win32 threading primitives ───

inline struct {
    const char* Titulo = AY_OBFUSCATE("Satella");
    std::vector<const char*> Colluns = { "Main", "Visuals", "Misc" };
    bool OverlayView = true;
    bool AtivarFuncoes = true;
    bool Progress = false;
    bool Attached = false;
    bool Autenticado = false;
    char Usuario[256] = "";
    char Senha[256] = "";
    char HWID[256] = "";
    bool MenuVisible = true;
} Auth;

inline struct {
    int Menu = VK_INSERT;
    int GhostHack;
    int RotatePlayer = 0;
    int SilentAim = 0;
} KeysBind;

// === AIMBOT ===
inline bool AimSilent = true;
inline bool AimVisibleCheck = true;
inline bool AimPrediction = true;
inline float AimPredictionAmount = 1.5f;
inline float AimFOV = 15.0f;
inline int AimBone = 0;
inline int AimKey = 0;
inline bool IgnoreKnocked = true;
inline bool IgnoreBots = true;
inline int DistanceAim = 200;
inline bool Triggerbot = false;
inline int TriggerbotDelay = 100;
inline bool BunnyHop = false;
inline bool AutoReload = false;
inline bool FastReload = false;

// === AIMBOT ===
inline bool RageAimEnabled = false;
inline int RageAimKey = VK_RBUTTON;
inline bool RageAimRequireKey = false;
inline int AimbotHitbox = 0; // 0=Head, 1=Body

// === LEGIT AIMBOT ===
inline bool LegitAimEnabled = false;
inline float LegitAimFOV = 8.0f;
inline float LegitAimSmoothness = 5.0f;
inline bool LegitAimShootOnly = true;
inline bool LegitAimVisibleCheck = true;
inline bool LegitAimPrediction = true;
inline float LegitAimPredictionAmount = 1.5f;
inline int LegitAimHitbox = 0; // 0=Body, 1=Head, 2=Neck, 3=Chest

// === NOVO AIMBOT (Configuravel) ===
inline bool AimbotLegit = true;
inline int AimbotKeyBind = 0;
inline int AimbotMaxDistance = 200;
inline bool AimbotIgnoreKnocked = false;
inline bool AimbotIgnoreBots = false;
inline int AimbotFOV = 180;
inline bool AimbotAimShoulder = false;
inline int AimbotPeitosIndex = 0;
inline uintptr_t AimbotTarget = 0;
inline float AimbotDistMax = 9999.9f;
inline bool AimbotNoRecoil = false;
inline bool AimbotPrecision = false;
inline float PrecisionAssist = 0.5f;

// === SILENT AIM ===
inline int SilentAimKeyBind = 0; // 0 = no key required
inline float SilentAimFOV = 15.0f;
inline int SilentAimDistance = 200;
inline int SilentAimHitbox = 0; // 0=head, 1=body
inline bool SilentAimIgnoreKnocked = true;
inline bool SilentAimIgnoreBots = true;

// === ESP ===
inline bool ESPNome = true;
inline bool ESPLinha = true;
inline bool ESPEsqueleto = true;
inline int ESPCaixa = 1;
inline bool ESPFilledBox = false;
inline bool ESPDistancia = true;
inline bool ESPHealthText = false;
inline int ESPHealthBarPos = 0; // 0=Off, 1=Left, 2=Right, 3=Top, 4=Bottom
inline bool ESPMostrarTime = false;
inline bool ESPMostrarDerrubado = false;
inline bool ESPWeaponName = false;
inline bool ESPEnemyCounter = false;
inline float espTextSize = 15.0f;
inline float espThickness = 1.5f;
inline float espBgAlpha = 0.15f;
inline float espMaxDistance = 250.0f;
inline int linePosition = 1;
inline float espOffsetX = 0.0f;
inline float espOffsetY = 0.0f;

// === MISC ===
inline bool ShowDebugConsole = false;
inline bool Watermark = true;
inline bool FPSCounter = true;
inline bool WeaponAttributesEnabled = false;
inline int WeaponAttributesLevel = 0; // 0=Lv1, 1=Lv2, 2=Lv3, 3=Lv4
inline bool SpinBot = false;
inline int SpinbotMode = 0; // 0=Continuous, 1=Random, 2=45 Step
inline float SpinbotSpeed = 5.0f;
inline bool ThirdPerson = false;
inline float ThirdPersonDist = 4.0f;
inline bool StreamMode = false;
inline bool StreamModeActive = false;
inline bool CrosshairEnabled = true;
inline bool HideTaskbar = false;
inline bool TopMost = false;
inline bool BypassAnticheat = false;
inline bool AimbotTrick = false;
inline bool PrecisionMode = false;

inline bool GhostHack = false;

// ─── Entity Cache Centralizado ───
struct EntityData;
struct UnityMatrix;
extern std::unordered_map<uintptr_t, EntityData>& GetEntityCache();
extern WinMutex& GetCacheWriteMutex();
extern uintptr_t cachedLocalPlayer;
extern UnityMatrix renderMatrix;
extern std::atomic<uint64_t> renderMatrixTimestamp;
extern std::atomic<int> g_entityCount;
extern uint64_t lastEntityUpdate;
extern int cachedScreenW, cachedScreenH;
void UpdateEntityCache();
void SwapEntityCache();
inline float CrosshairSize = 12.0f;
inline int CrosshairStyle = 0;
inline int ConfigSlot = 1;
inline bool NoRecoilEnabled = false;
inline bool NoSpreadEnabled = false;

inline float colorName[4] = {1.0f, 1.0f, 1.0f, 1.0f};
inline float colorLine[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
inline float colorSkeleton[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
inline float colorBox[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
inline float colorDistance[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
inline float colorDying[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
inline float colorCrosshair[4] = {1.0f, 1.0f, 1.0f, 1.0f};
inline float ParticleColour[4] = {0.0f, 150.0f / 255.0f, 1.0f, 1.0f};
inline ImVec4 Particle = ImVec4(0.0f, 150.0f / 255.0f, 1.0f, 1.0f);
inline std::atomic<int> CurrentTab = 0;

inline std::atomic<int> CurrentWindow = 0;
