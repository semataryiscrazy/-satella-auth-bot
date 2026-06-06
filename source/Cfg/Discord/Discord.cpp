#define _CRT_SECURE_NO_WARNINGS
#include "Discord.h"
#include <time.h>
#include <chrono>
#include "../strenc.h"

typedef void(__cdecl *fn_Discord_Initialize)(const char*, void*, int, const char*);
typedef void(__cdecl *fn_Discord_UpdatePresence)(const void*);
typedef void(__cdecl *fn_Discord_ClearPresence)(void);

static fn_Discord_Initialize _Discord_Initialize = nullptr;
static fn_Discord_UpdatePresence _Discord_UpdatePresence = nullptr;
static fn_Discord_ClearPresence _Discord_ClearPresence = nullptr;
static bool g_discordLoaded = false;

static void EnsureDiscordLoaded() {
    if (g_discordLoaded) return;
    HMODULE hMod = LoadLibraryW(L"discord-rpc.dll");
    if (hMod) {
        _Discord_Initialize = (fn_Discord_Initialize)GetProcAddress(hMod, "Discord_Initialize");
        _Discord_UpdatePresence = (fn_Discord_UpdatePresence)GetProcAddress(hMod, "Discord_UpdatePresence");
        _Discord_ClearPresence = (fn_Discord_ClearPresence)GetProcAddress(hMod, "Discord_ClearPresence");
    }
    g_discordLoaded = true;
}

static int64_t eptime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

void Discord::Initialize() {
    EnsureDiscordLoaded();
    if (!_Discord_Initialize) return;
    DiscordEventHandlers Handle;
    memset(&Handle, 0, sizeof(Handle));
    _Discord_Initialize(AY_OBFUSCATE("1312955273455079424"), &Handle, 1, NULL);
}

void Discord::Update(const char* User, int Dias) {
    if (!_Discord_UpdatePresence) return;
    DiscordRichPresence discordPresence;
    memset(&discordPresence, 0, sizeof(discordPresence));
    char detailsBuffer[128];
    sprintf_s(detailsBuffer, AY_OBFUSCATE("Usuario: %s"), User);
    discordPresence.details = detailsBuffer;
    char stateBuffer[128];
    sprintf_s(stateBuffer, AY_OBFUSCATE("Dias Restantes: %d"), Dias);
    discordPresence.state = stateBuffer;
    discordPresence.startTimestamp = eptime;
    discordPresence.largeImageKey = AY_OBFUSCATE("https://voidcorp.xyz/api/discord/icon.png");
    discordPresence.largeImageText = AY_OBFUSCATE("BlueStacks Version");
    _Discord_UpdatePresence(&discordPresence);
}

void Discord::Shutdown() {
    if (_Discord_ClearPresence) _Discord_ClearPresence();
}
