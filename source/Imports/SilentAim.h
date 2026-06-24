#pragma once
#include <atomic>
#include "../Unity/Vector3.h"

struct AimSettings {
    bool Enabled = false;
    int KeyBind = VK_LBUTTON;
    int HitBox = 0;
    float Fov = 180.0f;
    int UpdateMs = 3;
};

extern AimSettings g_Aim;

void StartSilentAim();
void StopSilentAim();
