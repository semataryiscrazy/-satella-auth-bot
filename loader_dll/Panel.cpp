#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include <dwmapi.h>
#include <chrono>
#include <thread>
#include <d3d11.h>

// Direct3D
static ID3D11Device* g_pd3dDevice = NULL;
static ID3D11DeviceContext* g_pd3dDeviceContext = NULL;
static IDXGISwapChain* g_pd3dSwapChain = NULL;
static ID3D11RenderTargetView* g_pd3dRenderTargetView = NULL;

// ImGui
#include "../source/Imports/Includes.h"
#include "LoginPanel.h"

// Forward declare the fonts from Main.cpp (we'll load our own)
extern void* InterRegular;
extern void* InterMedium;
extern void* InterBold;

// Global state
static bool g_done = false;
static LoginPanelState g_loginState;

// Forward declarations
LRESULT CALLBACK ImGuiWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void CreateImGuiWindow();
void RenderImGui();
void CleanupImGui();

// Entry point
extern "C" __declspec(dllexport) void ShowPanel() {
    // Create ImGui-enabled window
    CreateImGuiWindow();

    // Main loop
    auto last = std::chrono::steady_clock::now();
    while (!g_done) {
        // Process messages
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) {
                g_done = true;
            }
        }

        // Render ImGui at 60 FPS
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::microseconds>(now - last).count() >= 16666) { // ~60 FPS
            last = now;
            RenderImGui();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    CleanupImGui();
}

LRESULT CALLBACK ImGuiWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        ImGui_ImplDX11_ResizeSwapChain(LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

void CreateImGuiWindow() {
    // Window class
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, ImGuiWndProc, 0L, 0L,
                       GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
                       L"Satella ImGui Panel", NULL };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowExW(WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED,
                                  wc.lpszClassName, L"Satella Panel",
                                  WS_POPUP, 100, 100, 400, 340,
                                  NULL, NULL, wc.hInstance, NULL);

    // Make window click-through for mouse? No, we want it to be interactive.
    // Set layered attributes for transparency
    SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        return;
    }

    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Load fonts
    // We'll use the same fonts as the main cheat for consistency
    // In a real implementation, we would load the font data from memory or files.
    // For simplicity, we'll use the default font.
    // ImFont* font = io.Fonts->AddFontDefault();
    // IM_ASSERT(font != NULL);

    // Store hwnd globally for ImGui_ImplWin32_EnableDpiAwareness
    // (not needed for this example)

    // We'll keep the hwnd in a global or static if needed elsewhere
    // For now, we just need it for cleanup.
    // We'll use a static variable in this file.
    static HWND g_hwnd = hwnd;
}

void RenderImGui() {
    // Start the Dear ImGui frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Show our login panel
    bool done = RenderLoginPanel(g_loginState);
    if (done) {
        // User clicked Load or Bypass
        g_done = true;
        // In a real implementation, we would signal the cheat to start here.
        // For now, we just close the panel.
    }

    // Rendering
    ImGui::EndFrame();
    ImGui::Render();
    const float clear_color_with_alpha[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    g_pd3dDeviceContext->OMSetRenderTargets(1, &g_pd3dRenderTargetView, NULL);
    g_pd3dDeviceContext->ClearRenderTargetView(g_pd3dRenderTargetView, clear_color_with_alpha);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    g_pd3dDeviceContext->Present(1, 0); // Present with vsync
    //g_pd3dDeviceContext->Present(0, 0); // Present without vsync
}

// Helper functions for Direct3D (copied from imgui_impl_dx11.cpp example)
bool CreateDeviceD3D(HWND hWnd) {
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pd3dSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_WARP, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pd3dSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    // Create render target view
    ID3D11Texture2D* pBackBuffer;
    g_pd3dSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_pd3dRenderTargetView);
    pBackBuffer->Release();
    return true;
}

void CleanupDeviceD3D() {
    if (g_pd3dRenderTargetView) { g_pd3dRenderTargetView->Release(); g_pd3dRenderTargetView = NULL; }
    if (g_pd3dSwapChain) { g_pd3dSwapChain->Release(); g_pd3dSwapChain = NULL; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}

void CleanupImGui() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::UnregisterClass(L"Satella ImGui Panel", GetModuleHandle(NULL));
}