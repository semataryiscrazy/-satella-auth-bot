#pragma once

inline uint32_t TypeDefIndex(uintptr_t address) {
    uintptr_t inputPtr = Ler<uintptr_t>(address);
    if (inputPtr == 0) return 0;
    return Ler<uint32_t>(inputPtr + string2Offset(AY_OBFUSCATE("0xA4")));
}

inline uint32_t PlayerNetwork = 0x020072EF;
inline uint32_t PlayerUGCCommon = 0x02007B47;
inline uint32_t Player_TrainingHumanTarget_Stand = 0x02007B5C;
inline uint32_t Player_TrainingHumanTarget = 0x02007B5D;
