#define _CRT_SECURE_NO_WARNINGS
#pragma execution_character_set("utf-8")

#include <windows.h>
#include <iostream>
#include <thread>
#include <chrono>

// Include the main cheat header
#include "../source/Imports/Includes.h"
#include "../source/Main.h"
#include "../keyauth/ka_bridge.h"

// Include our login panel
#include "../source/Cfg/LoginPanel.h"

// Include Main.cpp (not compiled separately to avoid duplicate symbols)
#include "../source/Main.cpp"

// Standalone external memory reader (replaces VMM-based Ler/Escrever)
#include "MemoryExternal.h"

// Forward declarations from Main.cpp
void runRenderTick();
void DesenharESP(int width, int height);
void UnloadCheat();
void InitializeConsole();
extern bool g_Unload;

// ─── Standalone WinMain Entry Point ───
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 1. Initialize debug console (if enabled)
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONOUT$", "w", stderr);
    std::cout.clear();
    setvbuf(stdout, NULL, _IONBF, 0);
    SetConsoleOutputCP(CP_UTF8);
    HWND hConsole = GetConsoleWindow();
    if (hConsole) ShowWindow(hConsole, SW_SHOW); // Show console for debugging

    std::cout << "[Satella] Standalone overlay starting..." << std::endl;

    // 2. Initialize authentication early (KeyAuth)
    std::cout << "[Satella] KeyAuth configured for: satella" << std::endl;

    // 3. Initialize Discord RPC
    DiscordRPC = new Discord();
     
    // 4. Load saved keybinds from registry
    LoadKeyBinds();

    // 5. Setup the overlay window (calls imwindow.h's setupWindow)
    //    For standalone mode, pass NULL so it searches for BlueStacks/HD-Player window
    std::cout << "[Satella] Setting up overlay window..." << std::endl;
    setupWindow(NULL);
    
    if (!hwnd) {
        std::cerr << "[Satella] Failed to create overlay window!" << std::endl;
        MessageBoxA(NULL, "Failed to create overlay window.\nMake sure BlueStacks or HD-Player is running.", "Satella Overlay Error", MB_ICONERROR);
        return 1;
    }
    std::cout << "[Satella] Overlay window created successfully." << std::endl;

    // 6. Show login panel first, then enter render loop after successful login
    std::cout << "[Satella] Showing login panel..." << std::endl;
    bool showLoginPanel = true;
    LoginPanelState loginState;
    
    // Main loop
    using namespace std::chrono;
    auto lastRender = steady_clock::now();
    
    while (!g_Unload) {
        handleKeyPresses();
        
        auto now = steady_clock::now();
        if (showLoginPanel) {
            // Show login panel
            if (RenderLoginPanel(loginState)) {
                // User clicked Load or Bypass, start the cheat
                showLoginPanel = false;
                // Set authentication state so the render loop will start the cheat
                Auth.Autenticado = true;
                strcpy(Auth.Usuario, loginState.username);
                // Reset lastRender to avoid skipping a frame
                lastRender = now;
            }
        } else {
            // Cheat mode: run at 60 FPS
            if (duration_cast<nanoseconds>(now - lastRender).count() >= 16666666) {
                lastRender = now;
                __try {
                    runRenderTick();
                } __except(EXCEPTION_EXECUTE_HANDLER) {
                    std::cerr << "[Satella] Exception in render tick" << std::endl;
                }
            }
        }
        Sleep(1);
    }
    
    // 7. Cleanup
    std::cout << "[Satella] Shutting down..." << std::endl;
    
    if (DiscordRPC) {
        DiscordRPC->Shutdown();
        delete DiscordRPC;
        DiscordRPC = nullptr;
    }
    
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    delete[] g_Buffer; g_Buffer = nullptr; g_BufferWidth = g_BufferHeight = 0;
    if (hwnd) {
        SetWindowDisplayAffinity(hwnd, 0);
        ::DestroyWindow(hwnd);
    }
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);
    
    std::cout << "[Satella] Cleanup complete." << std::endl;
    return 0;
}
