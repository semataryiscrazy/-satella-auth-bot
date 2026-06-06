#include "LoginPanel.h"
#include <string.h>

// Forward declare the fonts from Main.cpp
extern void* InterRegular;
extern void* InterMedium;
extern void* InterBold;

// Forward declare the authentication functions
extern bool ka_init();
extern bool ka_login(const char* username, const char* password);
extern bool ka_register(const char* username, const char* password, const char* key);
extern const char* ka_get_error();
extern void NotificationManager_AdicionarNotificacao(const char* message);

// Simple wrapper for ImGui::InputTextEx3 from the original code
bool InputTextEx3(const char* label, const char* hint, char* buf, size_t buf_size, const ImVec2& size_arg, ImGuiInputTextFlags flags) {
    // This is a simplified version - in reality this would call the actual ImGui::InputTextEx3
    // For now we'll use the standard InputText which should work for basic functionality
    return ImGui::InputText(label, buf, buf_size, flags);
}

bool RenderLoginPanel(LoginPanelState& state) {
    // Set up window size and position
    ImGui::SetNextWindowSize(ImVec2(400, 340));
    ImGui::SetNextWindowPos(ImVec2(
        (ImGui::GetIO().DisplaySize.x - 400) * 0.5f,
        (ImGui::GetIO().DisplaySize.y - 340) * 0.5f
    ));
    
    // Begin the window with no decoration (like the original panel)
    if (!ImGui::Begin("Satella Loader", nullptr, 
                      ImGuiWindowFlags_NoDecoration | 
                      ImGuiWindowFlags_NoScrollWithMouse |
                      ImGuiWindowFlags_NoMove)) {
        ImGui::End();
        return false;
    }
    
    // Get window position and size for layout
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 windowSize = ImGui::GetWindowSize();
    
    // Background
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(windowPos, 
                           ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y),
                           IM_COL32(18, 18, 28, 240)); // Semi-transparent dark background
    
    // Layout constants
    int fx = 40;
    int fy = 95;
    int fw = windowSize.x - 80;
    int fh = 30;
    int bw = (fw - 10) / 2;
    int by = fy + fh * 2 + 46;
    
    // Draw title
    ImGui::PushFont(InterBold);
    ImGui::SetCursorPos(ImVec2(windowPos.x + windowSize.x * 0.5f - ImGui::CalcTextSize("Satella Loader").x * 0.5f, windowPos.y + 20));
    ImGui::TextColored(IM_COL32(255, 255, 255, 255), "Satella Loader");
    ImGui::PopFont();
    
    // Username field (if visible)
    if (state.showUsername) {
        ImGui::PushFont(InterRegular);
        ImGui::SetCursorPos(ImVec2(windowPos.x + fx, windowPos.y + fy - 20));
        ImGui::TextColored(IM_COL32(200, 200, 220, 200), "Username");
        ImGui::SetCursorPos(ImVec2(windowPos.x + fx, windowPos.y + fy));
        ImGui::InputTextEx3("##username", "Enter your username", state.username, sizeof(state.username), ImVec2(fw, fh), 0);
        ImGui::PopFont();
    }
    
    // Password field (if visible)
    if (state.showPassword) {
        ImGui::PushFont(InterRegular);
        ImGui::SetCursorPos(ImVec2(windowPos.x + fx, windowPos.y + fy + fh + 12));
        ImGui::TextColored(IM_COL32(200, 200, 220, 200), "Password");
        ImGui::SetCursorPos(ImVec2(windowPos.x + fx, windowPos.y + fy + fh + 28));
        ImGui::InputTextEx3("##password", "Enter your password", state.password, sizeof(state.password), ImVec2(fw, fh), ImGuiInputTextFlags_Password);
        ImGui::PopFont();
    }
    
    // Login button
    if (state.showLoginBtn) {
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(60, 80, 180, 220));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(80, 100, 220, 240));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(100, 120, 255, 255));
        
        ImGui::SetCursorPos(ImVec2(windowPos.x + fx, windowPos.y + by));
        if (ImGui::Button("Login", ImVec2(bw, 34))) {
            if (strlen(state.username) > 0 && strlen(state.password) > 0) {
                // Simulate login process
                if (ka_init() && ka_login(state.username, state.password)) {
                    state.isLoggedIn = true;
                    strcpy(state.statusText, "Welcome back!");
                    
                    // Hide login fields, show load/bypass buttons
                    state.showUsername = false;
                    state.showPassword = false;
                    state.showLoginBtn = false;
                    state.showRegisterBtn = false;
                    state.showLoadBtn = true;
                    state.showBypassBtn = true;
                    
                    // In a real implementation, we would start the cheat here
                    // NotificationManager::AdicionarNotificacao("Bem-Vindo, " + std::string(state.username) + "!");
                } else {
                    strcpy(state.statusText, ka_get_error() && ka_get_error()[0] ? ka_get_error() : "Falha no login");
                }
            } else {
                strcpy(state.statusText, "Preencha usuario e senha");
            }
        }
        ImGui::PopStyleColor(3);
    }
    
    // Register button
    if (state.showRegisterBtn) {
        ImGui::SetCursorPos(ImVec2(windowPos.x + fx + bw + 10, windowPos.y + by));
        if (ImGui::Button("Register", ImVec2(bw, 34))) {
            if (strlen(state.username) > 0 && strlen(state.password) > 0) {
                // In a real implementation, we would ask for a license key here
                // For now, we'll simulate registration
                state.isLoggedIn = true;
                strcpy(state.statusText, "Account created!");
                
                // Hide login fields, show load/bypass buttons
                state.showUsername = false;
                state.showPassword = false;
                state.showLoginBtn = false;
                state.showRegisterBtn = false;
                state.showLoadBtn = true;
                state.showBypassBtn = true;
            } else {
                strcpy(state.statusText, "Preencha usuario e senha");
            }
        }
    }
    
    // Status text
    if (strlen(state.statusText) > 0) {
        ImGui::SetCursorPos(ImVec2(windowPos.x + 20, windowPos.y + windowSize.y - 60));
        ImGui::TextColored(IM_COL32(200, 210, 230, 200), "%s", state.statusText);
    }
    
    // Load button (visible after login)
    if (state.showLoadBtn) {
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(60, 180, 80, 220));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(80, 220, 100, 240));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(100, 255, 120, 255));
        
        ImGui::SetCursorPos(ImVec2(windowPos.x + fx, windowPos.y + by));
        if (ImGui::Button("Load", ImVec2(bw, 34))) {
            strcpy(state.statusText, "Loading Satella...");
            // Here we would trigger the actual cheat loading
            return true; // Indicate that load was clicked
        }
        ImGui::PopStyleColor(3);
    }
    
    // Bypass button (visible after login)
    if (state.showBypassBtn) {
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(180, 180, 60, 220));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(220, 220, 80, 240));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(255, 255, 100, 255));
        
        ImGui::SetCursorPos(ImVec2(windowPos.x + fx + bw + 10, windowPos.y + by));
        if (ImGui::Button("Bypass", ImVec2(bw, 34))) {
            strcpy(state.statusText, "Bypass activated");
            // Here we would trigger the bypass functionality
            return true; // Indicate that bypass was clicked
        }
        ImGui::PopStyleColor(3);
    }
    
    ImGui::End();
    return false;
}