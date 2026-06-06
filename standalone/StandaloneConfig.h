#pragma once

// ─── Standalone Overlay Configuration ───
// Define this when building the standalone EXE (not the injected DLL)
#ifndef STANDALONE
#define STANDALONE
#endif

// ─── Build Settings ───
// Uncomment to use DX11 rendering instead of GDI
// #define USE_DX11_RENDERER

// Uncomment to keep Discord RPC (requires Discord SDK)
#define USE_DISCORD_RPC

// ─── Memory Reading Mode ───
// Uncomment to use external ReadProcessMemory (standalone)
// Comment out to use VMM physical memory reading (injected DLL only)
#define USE_EXTERNAL_MEMORY

// ─── Target Process for Memory Reading ───
// The process name to attach to for external memory reading
#define TARGET_PROCESS_NAME L"HD-Player.exe"
// Alternative: L"BlueStacks.exe" or L"BlueStacksApp.exe"

// ─── KeyAuth Credentials ───
#define KA_APP_NAME "satella"
#define KA_OWNER_ID "2muvrcPJ73"
#define KA_SECRET "ea66bf36a53fe812ae7713a9449cbfffebd4b65e48ca3f61bf6b9234e6737475"
#define KA_VERSION "1.0"
