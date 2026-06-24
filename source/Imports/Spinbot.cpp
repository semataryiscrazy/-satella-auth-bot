#include "Offsets.h"
#include "Utils.h"
#include "Scope.h"
#include "../Unity/Vector3.h"
#include "../Unity/Quaternion.h"
#include <chrono>

namespace SpinbotImpl {

    static float accumulatedAngle = 0.0f;
    static Quaternion initialRotation{};
    static bool wasRotating = false;

    void Execute(uintptr_t localPlayer) {
        if (!SpinBot || localPlayer == 0) {
            accumulatedAngle = 0.0f;
            wasRotating = false;
            return;
        }

        // Key bind check
        if (KeysBind.RotatePlayer != 0) {
            if ((GetAsyncKeyState(KeysBind.RotatePlayer) & 0x8000) == 0) {
                accumulatedAngle = 0.0f;
                return;
            }
        }

        // Stop when moving (WASD)
        if ((GetAsyncKeyState('W') & 0x8000) ||
            (GetAsyncKeyState('A') & 0x8000) ||
            (GetAsyncKeyState('S') & 0x8000) ||
            (GetAsyncKeyState('D') & 0x8000))
        {
            return;
        }

        uintptr_t m_CachedTransform = Ler<uintptr_t>(localPlayer + Offsets::MainTransform);
        if (m_CachedTransform == 0) return;

        uintptr_t TransformAcess = Ler<uintptr_t>(m_CachedTransform + 0x8);
        if (TransformAcess == 0) return;

        int TransformIndex = Ler<int>(TransformAcess + 0x24);
        uintptr_t TransformMatrix = Ler<uintptr_t>(TransformAcess + 0x20);
        if (TransformMatrix == 0) return;

        uintptr_t pTransformValues = Ler<uintptr_t>(TransformMatrix + 0x18);
        if (pTransformValues == 0) return;

        int rotationOffset = 0x30 * TransformIndex + 0x10;
        Quaternion currentRot = Ler<Quaternion>(pTransformValues + rotationOffset);
        if (currentRot.X == 0 && currentRot.Y == 0 && currentRot.Z == 0 && currentRot.W == 0) return;

        if (!wasRotating) {
            initialRotation = currentRot;
            accumulatedAngle = 0.0f;
            wasRotating = true;
        }

        switch (SpinbotMode) {
        case 0: // Continuous
            accumulatedAngle += SpinbotSpeed;
            break;
        case 1: // Random
            accumulatedAngle += static_cast<float>(rand() % 360);
            break;
        case 2: // 45° Step
            accumulatedAngle += 45.0f;
            break;
        }

        if (accumulatedAngle >= 360.0f)
            accumulatedAngle -= 360.0f;

        float totalAngleRad = accumulatedAngle * (3.14159265f / 180.0f);
        Quaternion deltaY = Quaternion::FromAngleAxis(totalAngleRad, Vector3(0, 1, 0));
        Quaternion newRot = Quaternion::Normalized(initialRotation * deltaY);

        Escrever<Quaternion>(pTransformValues + rotationOffset, newRot);
    }
}
