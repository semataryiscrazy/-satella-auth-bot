#pragma once
#include <cstdint>
#include "strenc.h"
#include "encrypt.hh"

class Offsets {
public:
    // ==========================================
    // 🔥 OFFSETS v7a - FF NORMAL 🔥
    // ==========================================
    
    // === BASE ===
    static inline uintptr_t Il2Cpp = 0x0;
    static inline uintptr_t UnityCpp = 0x0;
    static inline uintptr_t libunity = 0x0;
    static inline uintptr_t InitBase = string2Offset(AY_OBFUSCATE("0x9EC1C48"));
    static inline uintptr_t StaticClass = string2Offset(AY_OBFUSCATE("0x5C"));
    
    // === DICTIONARY ===
    static inline uintptr_t Il2CppDictionaryDataPtr = string2Offset(AY_OBFUSCATE("0xC"));
    static inline uintptr_t Il2CppDictionaryCount = string2Offset(AY_OBFUSCATE("0x10"));
    
    // === MATCH ===
    static inline uintptr_t CurrentMatch = string2Offset(AY_OBFUSCATE("0x50"));
    static inline uintptr_t MatchStatus = string2Offset(AY_OBFUSCATE("0x8C"));
    static inline uintptr_t LocalPlayer = string2Offset(AY_OBFUSCATE("0x94"));
    static inline uintptr_t DictionaryEntities = string2Offset(AY_OBFUSCATE("0x68"));
    static inline uintptr_t TimerPtr = string2Offset(AY_OBFUSCATE("0x10"));
    static inline uintptr_t Timer = string2Offset(AY_OBFUSCATE("0x24"));
    
    // === PLAYER ===
    static inline uintptr_t Player_IsDead = string2Offset(AY_OBFUSCATE("0x50"));
    static inline uintptr_t Player_Name = string2Offset(AY_OBFUSCATE("0x2E4"));
    static inline uintptr_t Player_Data = string2Offset(AY_OBFUSCATE("0x48"));
    static inline uintptr_t ShadowState = string2Offset(AY_OBFUSCATE("0x15E8"));
    static inline uintptr_t Player_ShadowBase = string2Offset(AY_OBFUSCATE("0x16BC"));
    static inline uintptr_t XPose = string2Offset(AY_OBFUSCATE("0x78"));
    static inline uintptr_t YPose = string2Offset(AY_OBFUSCATE("0x7C"));
    static inline uintptr_t ZPose = string2Offset(AY_OBFUSCATE("0x80"));
    static inline uintptr_t PlayerPosition = string2Offset(AY_OBFUSCATE("0x78")); // Posição completa do player (Vector3)
    
    // === AVATAR ===
    static inline uintptr_t AvatarManager = string2Offset(AY_OBFUSCATE("0x4C4"));
    static inline uintptr_t Avatar = string2Offset(AY_OBFUSCATE("0xA0"));
    static inline uintptr_t Avatar_IsVisible = string2Offset(AY_OBFUSCATE("0x95"));
    static inline uintptr_t Avatar_Data = string2Offset(AY_OBFUSCATE("0x14"));
    static inline uintptr_t Avatar_Data_IsTeam = string2Offset(AY_OBFUSCATE("0x59"));
    static inline uintptr_t Avatar_Data_IsBot = string2Offset(AY_OBFUSCATE("0x2EC"));
    
    static inline uintptr_t PlayerRotation = string2Offset(AY_OBFUSCATE("0xFC"));
    
    // === CAMERA ===
    static inline uintptr_t FollowCamera = string2Offset(AY_OBFUSCATE("0x454"));
    static inline uintptr_t Camera = string2Offset(AY_OBFUSCATE("0x18"));
    static inline uintptr_t AimRotation = string2Offset(AY_OBFUSCATE("0x404")); // Quaternion (4 floats) OB53
    static inline uintptr_t AimRotationCheck = string2Offset(AY_OBFUSCATE("0x3B8"));
    static inline uintptr_t AuxAimRotation = string2Offset(AY_OBFUSCATE("0x3B8"));
    static inline uintptr_t MainCameraTransform = string2Offset(AY_OBFUSCATE("0x254"));
    static inline uintptr_t ViewMatrix = string2Offset(AY_OBFUSCATE("0xE8"));
    
    // === WEAPON ===
    static inline uintptr_t Weapon = string2Offset(AY_OBFUSCATE("0x3F8"));
    static inline uintptr_t WeaponFallback = string2Offset(AY_OBFUSCATE("0x35C"));
    static inline uintptr_t WeaponData = string2Offset(AY_OBFUSCATE("0x58"));
    static inline uintptr_t WeaponRecoil = string2Offset(AY_OBFUSCATE("0xC"));
    static inline uintptr_t WeaponOnHand = string2Offset(AY_OBFUSCATE("0x4C"));
    static inline uintptr_t InventoryManager = string2Offset(AY_OBFUSCATE("0x4AC"));
    static inline uintptr_t m_itemOnHand = string2Offset(AY_OBFUSCATE("0x54"));
    static inline uintptr_t m_FireDuration = string2Offset(AY_OBFUSCATE("0x4B4"));
    
    // === PLAYER ATTRIBUTES ===
    static inline uintptr_t LocalPlayerAttributes = string2Offset(AY_OBFUSCATE("0x4C0"));
    // Se for igual a Bones::Head, NoReload não pode usar só este offset no local player (colisão).
    static inline uintptr_t PlayerAttributes = string2Offset(AY_OBFUSCATE("0x45C"));
    static inline uintptr_t PlayerAttributes_Ptr = string2Offset(AY_OBFUSCATE("0x45C")); // Ponteiro para PlayerAttributes
    static inline uintptr_t LevelUp = string2Offset(AY_OBFUSCATE("0x14A8"));
    
    // === DAMAGE ATTRIBUTES (dentro de PlayerAttributes) ===
    static inline uintptr_t DamageAdditionScale = string2Offset(AY_OBFUSCATE("0x10")); // Escala de dano adicional
    static inline uintptr_t ExecuteDamageScale = string2Offset(AY_OBFUSCATE("0x14")); // Escala de execução de dano
    
    // === BOT ===
    static inline uintptr_t IsClientBot = string2Offset(AY_OBFUSCATE("0x2EC"));
    
    // === FIRING ===
    static inline uintptr_t IsFiring = string2Offset(AY_OBFUSCATE("0x544"));
    static inline uintptr_t LocalPlayerIsFiring = string2Offset(AY_OBFUSCATE("0x544"));
    
    // === AIMBOT COLLIDERS ===
    static inline uintptr_t HeadCollider = string2Offset(AY_OBFUSCATE("0x4A8"));
    static inline uintptr_t HeadCollider_Alt = string2Offset(AY_OBFUSCATE("0x4A8"));
    static inline uintptr_t Player_TargetCollider = string2Offset(AY_OBFUSCATE("0x54"));
    static inline uintptr_t ReplaceCollider = string2Offset(AY_OBFUSCATE("0x54"));
    static inline uintptr_t ColliderHECFNHJKOMN = string2Offset(AY_OBFUSCATE("0x4A8"));
    static inline uintptr_t ColliderINICDNFOFJB = string2Offset(AY_OBFUSCATE("0x54"));
    
    // === RELOAD ===
    static inline uintptr_t NoReload = string2Offset(AY_OBFUSCATE("0x99"));
    static inline uintptr_t NoReload2 = string2Offset(AY_OBFUSCATE("0x91"));
    
    // === SILENT AIM ===
    static inline uintptr_t Sillent = string2Offset(AY_OBFUSCATE("0x948"));                // MADMMIICBNN AimInfo ptr (0x8F0 old, 0x948 new)
    static inline uintptr_t AimInfo_RayDir = string2Offset(AY_OBFUSCATE("0x2C"));         // Vector3 bullet direction (inside AimInfo)
    static inline uintptr_t AimInfo_StartPos = string2Offset(AY_OBFUSCATE("0x38"));       // Vector3 gun muzzle pos (inside AimInfo)
    static inline uintptr_t AimInfo_HitPos = string2Offset(AY_OBFUSCATE("0x20"));         // Vector3 hit position (OB53: 0x20)
    static inline uintptr_t AimInfo_Distance = string2Offset(AY_OBFUSCATE("0x48"));       // float distance
    static inline uintptr_t Scatter_Weapon = string2Offset(AY_OBFUSCATE("0x6C"));         // Scatter struct ptr (inside weapon)
    
    // === WEAPON DATA (Weapon + 0x58) ===
    static inline uintptr_t WD_Damage = string2Offset(AY_OBFUSCATE("0x0"));
    static inline uintptr_t WD_DamageIncrease = string2Offset(AY_OBFUSCATE("0x4"));
    static inline uintptr_t WD_AmmoClipSize = string2Offset(AY_OBFUSCATE("0x14"));
    static inline uintptr_t WD_FireInterval = string2Offset(AY_OBFUSCATE("0x1C"));
    static inline uintptr_t WD_RepeatFireInterval = string2Offset(AY_OBFUSCATE("0x24"));
    static inline uintptr_t WD_MultiFireInterval = string2Offset(AY_OBFUSCATE("0x28"));
    static inline uintptr_t WD_AttachmentFireInterval = string2Offset(AY_OBFUSCATE("0x30"));
    static inline uintptr_t WD_BiteArmor = string2Offset(AY_OBFUSCATE("0x38"));
    static inline uintptr_t WD_Range = string2Offset(AY_OBFUSCATE("0x40"));
    static inline uintptr_t WD_ReloadSpeed = string2Offset(AY_OBFUSCATE("0x84"));
    static inline uintptr_t WD_IsSingleShot = string2Offset(AY_OBFUSCATE("0xA8"));
    static inline uintptr_t WD_DamageHead = string2Offset(AY_OBFUSCATE("0xAC"));
    static inline uintptr_t WD_DamageLimb = string2Offset(AY_OBFUSCATE("0xB0"));
    static inline uintptr_t WD_PrefireDelay = string2Offset(AY_OBFUSCATE("0x140"));

    // === EXTRAS ===
    static inline uintptr_t Vida = string2Offset(AY_OBFUSCATE("0x10"));
    static inline uintptr_t pomba = string2Offset(AY_OBFUSCATE("0x488"));
    static inline uintptr_t bisteca = string2Offset(AY_OBFUSCATE("0x854"));
    static inline uintptr_t arma = string2Offset(AY_OBFUSCATE("0x38"));
    static inline uintptr_t tiro = string2Offset(AY_OBFUSCATE("0x2c"));
    static inline uintptr_t FireInterval = string2Offset(AY_OBFUSCATE("0x188"));
    static inline uintptr_t Weapon_Direction = string2Offset(AY_OBFUSCATE("0x2c"));
    static inline uintptr_t FastSwitch = string2Offset(AY_OBFUSCATE("0x4BC"));
    static inline uintptr_t telepneu = string2Offset(AY_OBFUSCATE("0x0"));
    static inline uintptr_t GhostMode = string2Offset(AY_OBFUSCATE("0x524"));  // CORRETO - Ghost flag
    static inline uintptr_t Ghost = string2Offset(AY_OBFUSCATE("0x524"));      // Alias para GhostMode
    static inline uintptr_t MainTransform = string2Offset(AY_OBFUSCATE("0x38")); // Transform principal do jogador
    static inline uintptr_t CDOBMFNCJHD = string2Offset(AY_OBFUSCATE("0x7C1")); // flag personagem feminino (bool)
    static inline uintptr_t AIDDOCAPFKA = string2Offset(AY_OBFUSCATE("0x754")); // lista de bones (CapsuleCollider list)
    static inline uintptr_t ListEntities = string2Offset(AY_OBFUSCATE("0x13C")); // lista de entidades na partida
    static inline uintptr_t UmaAvatarSimple = string2Offset(AY_OBFUSCATE("0xA0"));
    static inline uintptr_t UMAData = string2Offset(AY_OBFUSCATE("0x14"));
    static inline uintptr_t TeamMate = string2Offset(AY_OBFUSCATE("0x59"));
    static inline uintptr_t PRIDataPool = string2Offset(AY_OBFUSCATE("0x48"));
    static inline uintptr_t ReplicationDataPoolUnsafe = string2Offset(AY_OBFUSCATE("0x8"));
    static inline uintptr_t ReplicationDataUnsafe = string2Offset(AY_OBFUSCATE("0x10"));
    static inline uintptr_t Health = string2Offset(AY_OBFUSCATE("0x10"));
    static inline uintptr_t LPEIEILIKGC = string2Offset(AY_OBFUSCATE("0x478"));
    static inline uintptr_t GameTimer = string2Offset(AY_OBFUSCATE("0x10"));
    static inline uintptr_t GameVariables = string2Offset(AY_OBFUSCATE("0xB0"));
    
    /** Reserva: analógico virtual (float); Spin Bot atual não usa. */
    static inline uintptr_t JoystickAnalogX = 0;
    static inline uintptr_t JoystickAnalogY = 0;
    
    /** Posição/rotação no IL2CPP TransformInternal (dump); não confundir com TMatrix da lista Unity. */
    static inline uintptr_t Position = string2Offset(AY_OBFUSCATE("0x7"));
    static inline uintptr_t Rotation = string2Offset(AY_OBFUSCATE("0x39"));
    
    /** Spin Bot: quaternion dentro do TMatrix local (após Vector4 posição = 16 bytes). */
    static inline uintptr_t SpinBotMatrixRotation = string2Offset(AY_OBFUSCATE("0x10"));
    
    /** Spin Bot (cadeia Root+8+8+20): offset do quaternion na estrutura apontada por matrix. */
    static inline uintptr_t SpinBotChainQuaternion = string2Offset(AY_OBFUSCATE("0x70"));
    static inline uintptr_t ParachuteDragA = string2Offset(AY_OBFUSCATE("0x1A8"));
    static inline uintptr_t ParachuteDragB = string2Offset(AY_OBFUSCATE("0x1F4"));
    static inline uintptr_t CurrentObserver = string2Offset(AY_OBFUSCATE("0x64"));
    static inline uintptr_t ObserverPlayer = string2Offset(AY_OBFUSCATE("0x28"));
    static inline uintptr_t RightcameraOffset = string2Offset(AY_OBFUSCATE("0x38"));
    static inline uintptr_t FOVcameraoffset = string2Offset(AY_OBFUSCATE("0x44"));
    static inline uintptr_t Backcameraoffset = string2Offset(AY_OBFUSCATE("0x40"));
    static inline uintptr_t Upcameraoffset = string2Offset(AY_OBFUSCATE("0x3c"));
    static inline uintptr_t Phase1CameraEulerAnglesY = string2Offset(AY_OBFUSCATE("0x48"));
    
    // ==========================================
    // 🦴 BONES v7a
    // ==========================================
    class Bones {
    public:
        static inline uintptr_t Head = string2Offset(AY_OBFUSCATE("0x45C"));
        static inline uintptr_t Neck = string2Offset(AY_OBFUSCATE("0x464"));
        static inline uintptr_t Hip = string2Offset(AY_OBFUSCATE("0x460"));
        static inline uintptr_t Pelvis = string2Offset(AY_OBFUSCATE("0x468"));
        static inline uintptr_t Hip2 = string2Offset(AY_OBFUSCATE("0x46C"));
        static inline uintptr_t Root = string2Offset(AY_OBFUSCATE("0x470"));
        static inline uintptr_t RootBone = string2Offset(AY_OBFUSCATE("0x474"));
        static inline uintptr_t LeftShoulder = string2Offset(AY_OBFUSCATE("0x490"));
        static inline uintptr_t RightShoulder = string2Offset(AY_OBFUSCATE("0x494"));
        static inline uintptr_t LeftElbow = string2Offset(AY_OBFUSCATE("0x4A4"));
        static inline uintptr_t RightElbow = string2Offset(AY_OBFUSCATE("0x4A0"));
        static inline uintptr_t LeftHand = string2Offset(AY_OBFUSCATE("0x49C"));
        static inline uintptr_t RightHand = string2Offset(AY_OBFUSCATE("0x458"));
        static inline uintptr_t LeftWrist = string2Offset(AY_OBFUSCATE("0x498"));
        static inline uintptr_t RightWrist = string2Offset(AY_OBFUSCATE("0x498"));
        static inline uintptr_t LeftCalf = string2Offset(AY_OBFUSCATE("0x478"));
        static inline uintptr_t LeftFoot = string2Offset(AY_OBFUSCATE("0x478"));
        static inline uintptr_t RightCalf = string2Offset(AY_OBFUSCATE("0x47C"));
        static inline uintptr_t RightFoot = string2Offset(AY_OBFUSCATE("0x47C"));
        static inline uintptr_t LeftAnkle = string2Offset(AY_OBFUSCATE("0x478"));
        static inline uintptr_t RightAnkle = string2Offset(AY_OBFUSCATE("0x47C"));
        static inline uintptr_t LeftKnee = string2Offset(AY_OBFUSCATE("0x484"));
        static inline uintptr_t RightKnee = string2Offset(AY_OBFUSCATE("0x488"));
        static inline uintptr_t Spine = string2Offset(AY_OBFUSCATE("0x48C"));
        static inline uintptr_t Chest = string2Offset(AY_OBFUSCATE("0x464"));
        
        // Offset dentro do node para posição world (Vector3)
        static inline uintptr_t Node_WorldPos = string2Offset(AY_OBFUSCATE("0x28"));
        static inline uintptr_t Node_Position = string2Offset(AY_OBFUSCATE("0x28"));
    };
};
