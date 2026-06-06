# Satella - Standalone Overlay Port

## Overview
This port converts the Satella cheat from a DLL injected into BlueStacks into a standalone Windows overlay application (EXE).

## Architecture Changes

### Before (DLL Injection)
- **Entry**: DllMain -> InitIdow() -> setupWindow() -> RenderLoop()
- **Memory**: VMM physical memory reading via BstkVMM.dll (embedded in BlueStacks)
- **Reading**: Ler<T>() / Escrever<T>() templates translate Android virtual addresses through physical memory
- **Anti-debug**: StopKellerETW(), ClearPEBDebugFlags(), NtSetInformationThread()
- **Target**: Injected into BlueStacks.exe process

### After (Standalone EXE)
- **Entry**: WinMain -> setupWindow() -> Custom render loop
- **Memory**: External ReadProcessMemory / WriteProcessMemory via MemoryExternal.h
- **Reading**: Direct process memory reading (Windows-native addresses)
- **Anti-debug**: Removed (not needed in standalone overlay)
- **Target**: Standalone layered window overlaid on BlueStacks/HD-Player

## Key Files Created

### standalone/SatellaOverlay.cpp (NEW)
WinMain entry point that:
- Initializes debug console
- Creates Discord RPC instance
- Loads saved keybinds from registry
- Calls setupWindow() to create the overlay
- Runs the ~60 FPS render loop
- Handles cleanup on shutdown

### standalone/MemoryExternal.h (NEW)
Singleton external memory reader that:
- Attaches to a target Windows process by name or PID
- Provides Read<T>(address) / Write<T>(address, value) templates
- Resolves module base addresses in the target process
- Replaces the old VMM-based Ler<T>() / Escrever<T>() via macro override

### standalone/StandaloneConfig.h (NEW)
Build configuration header with:
- STANDALONE preprocessor define
- TARGET_PROCESS_NAME for the process to read from
- KeyAuth credentials (duplicated from ka_bridge.cpp for convenience)

### standalone/SatellaOverlay.vcxproj (NEW)
Visual Studio 2022 project file configured as:
- Application (.exe) instead of DynamicLibrary (.dll)
- STANDALONE preprocessor define
- Same third-party library dependencies as the original
- Includes all original source files + new standalone files

### compile_standalone.bat (NEW)
Build script using MSBuild.

## Files Used From Original Project (unchanged)

| File | Purpose |
|------|---------|
| source/Main.cpp | ESP rendering (DesenharESP), UI logic (unRenderTick), aimbot integration |
| source/Cfg/imwindow.h | setupWindow(), GDI overlay renderer, WndProc, font loading, keybinds |
| source/Cfg/fonts.hpp | Font pointer declarations |
| source/Imports/Scope.h | Global state: Auth, KeysBind, aimbot flags, ESP flags, colors |
| source/Imports/Utils.h | Utility functions, registry helpers, window lookups |
| source/Imports/Offsets.h | Game structure offsets (FF v7a) |
| source/Imports/Process.h | Transform reading, bone positions, WorldToScreen |
| source/Imports/EntityCache.cpp | Entity caching loop (reads game entities) |
| source/Imports/SilentAim.cpp/h | Aimbot threads (silent, rage, no-recoil) |
| keyauth/ka_bridge.cpp | KeyAuth HTTP bridge (already standalone-capable) |
| source/Unity/*.h | Unity vector/matrix math types |
| source/Cfg/imgui/ | Dear ImGui library |
| source/Cfg/Discord/ | Discord RPC integration |

## Files Not Used / Removed in Standalone

| Feature | Reason |
|---------|--------|
| DllMain | Replaced by WinMain |
| InitIdow() | Startup logic moved to WinMain |
| StopKellerETW() | Anti-debug, not needed |
| ClearPEBDebugFlags() | Anti-debug, not needed |
| NtSetInformationThread() | Anti-debug, not needed |
| Guard process killing (KellerGuard, etc.) | Not needed in standalone overlay |
| deep_clean_internal() | Forensic cleanup, not needed |
| DLL section zeroing | DLL-specific anti-dump, not needed |
| VMM module (BstkVMM.dll) | Replaced by ReadProcessMemory |
| LoadLibraryAndHook() | Was used to hook PGMPhysRead - not needed |
| UnloadHooks() | MinHook cleanup, not needed |
| NetworkInit() / ConnectEmulator() | ADB-based setup - can be kept or adapted |

## CRITICAL: Memory Reading - The Remaining Challenge

The original code reads game data (entity positions, health, bones, view matrix, etc.) from the Android game process running **inside** BlueStacks emulator. It does this via:

1. **ADB** to find the game PID and module bases inside Android
2. **VMM** (BstkVMM.dll) to translate Android virtual addresses -> guest physical addresses
3. **PGMPhysRead** to read from the emulator's physical memory

For the standalone overlay, you have several options:

### Option A: ADB-based reading (slow but works from any EXE)
Read via ADB shell commands using dd from /proc/pid/mem:
`cpp
adb shell su -c "dd if=/proc/pid/mem bs=4 count=1 skip=\ 2>/dev/null | xxd -p"
`
This is slow (~50 reads/sec) but requires no kernel access.

### Option B: VMM from standalone process
Load BstkVMM.dll into the standalone overlay process (it's a legitimate BlueStacks DLL):
`cpp
HMODULE vmm = LoadLibraryA("BstkVMM.dll");
// Then use PGMPhysRead/PGMPhysWrite as before
`

### Option C: External emulator process memory map
BlueStacks maps guest physical memory into its process space. You can find and read it via:
`cpp
// Read /proc/pid/maps from inside BlueStacks via ADB to find the physical memory mapping
// Then ReadProcessMemory on the BlueStacks process at those mapped addresses
`

### Recommended: External ADB + Process Approach
1. Keep the ADB-based module discovery (works from any process)
2. Use ADB shell to read/write game memory from the Android process
3. This is slower but avoids needing VMM kernel access

## How to Build

### Prerequisites
- Visual Studio 2022 (v143 toolset)
- Windows 10/11 SDK
- DirectX SDK (for D3D11)

### Steps
1. Clone the repository
2. Open standalone/SatellaOverlay.vcxproj or use the .sln
3. Build for Release|x64
4. Output: x64\Release\SatellaOverlay.exe

### Manual Build
`
compile_standalone.bat
`

## Remaining Work / TODO

1. **Memory Backend**: Implement the actual memory reading for Android game inside BlueStacks
2. **MinHook Dependency**: Remove or stub MinHook for standalone (currently includes MinHook.h in Includes.h)
3. **Process Attachment**: Add BlueStacks process detection and attachment in WinMain
4. **Test GDI Renderer**: The GDI overlay renderer in imwindow.h works standalone but may need performance tuning
5. **Discord RPC**: Optionally enable/disable via config
6. **External Entity Cache**: The EntityCache.cpp uses Ler<T>() - will work with external memory reader once backend is implemented
7. **Aimbot Memory Writes**: SilentAim.cpp uses Escrever<T>() for aim correction - needs emulator write access

