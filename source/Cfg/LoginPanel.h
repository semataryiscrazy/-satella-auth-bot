#pragma once

#include <string>
#include <imgui.h>

bool InputTextEx3(const char* label, const char* hint, char* buf, size_t buf_size, const ImVec2& size_arg, ImGuiInputTextFlags flags);

struct LoginPanelState {
    bool isLoggedIn = false;
    char username[64] = "";
    char password[64] = "";
    char statusText[64] = "";
    bool showUsername = true;
    bool showPassword = true;
    bool showLoginBtn = true;
    bool showRegisterBtn = true;
    bool showLoadBtn = false;
    bool showBypassBtn = false;
};

bool RenderLoginPanel(LoginPanelState& state);
