//#define FFMax

class Offsets {
public:
#ifdef FFMax

    static inline uintptr_t Il2Cpp = 0x0;
    static inline uintptr_t InitBase = 0x9EC1C48;           // ALTERADO (era 0xA3F438C)
    static inline uintptr_t StaticClass = 0x5C;             // ALTERADO (era 0x5c)

    static inline uintptr_t CurrentMatch = 0x50;            // m_Match
    static inline uintptr_t MatchStatus = 0x8C;             // ALTERADO (era 0x74)
    static inline uintptr_t LocalPlayer = 0x94;             // ALTERADO (era 0x7C)
    static inline uintptr_t DictionaryEntities = 0x68;      // m_ReplicationEntitis

    static inline uintptr_t Player_IsDead = 0x50;           // ALTERADO (era 0x4C)
    static inline uintptr_t Player_Name = 0x2E4;            // ALTERADO (era 0x28C)
    static inline uintptr_t Player_Data = 0x48;             // ALTERADO (era 0x44)

    static inline uintptr_t Player_ShadowBase = 0x16BC;     // ALTERADO (era 0x15E8)
    static inline uintptr_t XPose = 0x78;                   // TargetPhysXPose

    static inline uintptr_t AvatarManager = 0x4C4;          // ALTERADO (era 0x460)
    static inline uintptr_t Avatar = 0xA0;                  // ALTERADO (era 0x94)
    static inline uintptr_t Avatar_IsVisible = 0x95;        // ALTERADO (era 0x7C)
    static inline uintptr_t Avatar_Data = 0x14;             // ALTERADO (era 0x10)
    static inline uintptr_t Avatar_Data_IsTeam = 0x59;      // ALTERADO (era 0x51)

    static inline uintptr_t FollowCamera = 0x454;           // ALTERADO (era 0x3F0)
    static inline uintptr_t Camera = 0x18;                  // ALTERADO (era 0x14)
    static inline uintptr_t AimRotation = 0x404;            // ALTERADO (era 0x3A8)
    static inline uintptr_t MainCameraTransform = 0x254;    // ALTERADO (era 0x1FC)

    static inline uintptr_t Weapon = 0x3F8;                 // ALTERADO (era 0x35C)
    static inline uintptr_t WeaponData = 0x58;              // mantido (mesmo valor)
    static inline uintptr_t WeaponRecoil = 0xC;             // mantido
    static inline uintptr_t WeaponOnHand = 0x4C;            // mantido (não existia na V7A)

    static inline uintptr_t ViewMatrix = 0xE8;       // mantido (extra seu)

    static inline uintptr_t IsClientBot = 0x2EC;            // ALTERADO (era 0x254)

    static inline uintptr_t ColliderHECFNHJKOMN = 0x4A8;    // ALTERADO (era 0x444)
    static inline uintptr_t ColliderINICDNFOFJB = 0x54;     // mantido
    static inline uintptr_t Vida = 0xC;                     // ALTERADO (era 0x10)

    // Offsets seus mantidos (não existiam na V7A oficial)
    static inline uintptr_t pomba = 0x488;
    static inline uintptr_t bisteca = 0x854;
    static inline uintptr_t arma = 0x38;
    static inline uintptr_t tiro = 0x48C;

    static inline uintptr_t telepneu = 0x0;
    static inline uintptr_t GhostMode = 0x38;               // ALTERADO (era 0X38 - só maiúsculo)
    static inline uintptr_t LPEIEILIKGC = 0x478;

    static inline uintptr_t PlayerAttributes = 0x4C0;       // ALTERADO (era 0x404) - LocalPlayerAttributes na V7A
    static inline uintptr_t NoReload2 = 0x91;               // mantido
    static inline uintptr_t GameTimer = 0x10;               // ALTERADO (era 0x24)

    static inline uintptr_t GameVariables = 0xb0;           // mantido

    static inline uintptr_t CurrentObserver = 0x64;         // mantido
    static inline uintptr_t ObserverPlayer = 0x28;          // mantido

    static inline uintptr_t RightcameraOffset = 0x38;       // mantido
    static inline uintptr_t FOVcameraoffset = 0x44;         // mantido
    static inline uintptr_t Backcameraoffset = 0x40;        // mantido
    static inline uintptr_t Upcameraoffset = 0x3c;          // mantido
    static inline uintptr_t Phase1CameraEulerAnglesY = 0x48;// mantido

    class Bones {
    public:
        static inline uintptr_t Head = 0x45C;   // ALTERADO (era 0x3F8)
        static inline uintptr_t Neck = 0x464;   // ALTERADO (era 0x400)  // Spine na V7A
        static inline uintptr_t LeftShoulder = 0x490;   // ALTERADO (era 0x42C)
        static inline uintptr_t RightShoulder = 0x494;   // ALTERADO (era 0x430)
        static inline uintptr_t LeftElbow = 0x4A4;   // ALTERADO (era 0x440)
        static inline uintptr_t RightElbow = 0x4A0;   // ALTERADO (era 0x43C)
        static inline uintptr_t LeftWrist = 0x49C;   // ALTERADO (era 0x3F4)
        static inline uintptr_t RightWrist = 0x498;   // ALTERADO (era 0x424)
        static inline uintptr_t Hip = 0x460;   // ALTERADO (era 0x3FC)
        static inline uintptr_t RightAnkle = 0x47C;   // ALTERADO (era 0x418)
        static inline uintptr_t LeftAnkle = 0x478;   // ALTERADO (era 0x414)
        static inline uintptr_t Root = 0x470;   // ALTERADO (era 0x40C)
    };
#else
    static inline uintptr_t Il2Cpp = 0x0;
    static inline uintptr_t InitBase = 0x9EC1C48;           // ALTERADO (era 0xA3F438C)
    static inline uintptr_t StaticClass = 0x5C;             // ALTERADO (era 0x5c)

    static inline uintptr_t CurrentMatch = 0x50;            // m_Match
    static inline uintptr_t MatchStatus = 0x8C;             // ALTERADO (era 0x74)
    static inline uintptr_t LocalPlayer = 0x94;             // ALTERADO (era 0x7C)
    static inline uintptr_t DictionaryEntities = 0x68;      // m_ReplicationEntitis

    static inline uintptr_t Player_IsDead = 0x50;           // ALTERADO (era 0x4C)
    static inline uintptr_t Player_Name = 0x2E4;            // ALTERADO (era 0x28C)
    static inline uintptr_t Player_Data = 0x48;             // ALTERADO (era 0x44)

    static inline uintptr_t Player_ShadowBase = 0x16BC;     // ALTERADO (era 0x15E8)
    static inline uintptr_t XPose = 0x78;                   // TargetPhysXPose

    static inline uintptr_t AvatarManager = 0x4C4;          // ALTERADO (era 0x460)
    static inline uintptr_t Avatar = 0xA0;                  // ALTERADO (era 0x94)
    static inline uintptr_t Avatar_IsVisible = 0x95;        // ALTERADO (era 0x7C)
    static inline uintptr_t Avatar_Data = 0x14;             // ALTERADO (era 0x10)
    static inline uintptr_t Avatar_Data_IsTeam = 0x59;      // ALTERADO (era 0x51)

    static inline uintptr_t FollowCamera = 0x454;           // ALTERADO (era 0x3F0)
    static inline uintptr_t Camera = 0x18;                  // ALTERADO (era 0x14)
    static inline uintptr_t AimRotation = 0x404;            // ALTERADO (era 0x3A8)
    static inline uintptr_t MainCameraTransform = 0x254;    // ALTERADO (era 0x1FC)

    static inline uintptr_t Weapon = 0x3F8;                 // ALTERADO (era 0x35C)
    static inline uintptr_t WeaponData = 0x58;              // mantido (mesmo valor)
    static inline uintptr_t WeaponRecoil = 0xC;             // mantido
    static inline uintptr_t WeaponOnHand = 0x4C;            // mantido (não existia na V7A)

    static inline uintptr_t ViewMatrix = 0xE8;       // mantido (extra seu)

    static inline uintptr_t IsClientBot = 0x2EC;            // ALTERADO (era 0x254)

    static inline uintptr_t ColliderHECFNHJKOMN = 0x4A8;    // ALTERADO (era 0x444)
    static inline uintptr_t ColliderINICDNFOFJB = 0x54;     // mantido
    static inline uintptr_t Vida = 0xC;                     // ALTERADO (era 0x10)

    // Offsets seus mantidos (não existiam na V7A oficial)
    static inline uintptr_t pomba = 0x488;
    static inline uintptr_t bisteca = 0x854;
    static inline uintptr_t arma = 0x38;
    static inline uintptr_t tiro = 0x48C;

    static inline uintptr_t telepneu = 0x0;
    static inline uintptr_t GhostMode = 0x38;               // ALTERADO (era 0X38 - só maiúsculo)
    static inline uintptr_t LPEIEILIKGC = 0x478;

    static inline uintptr_t PlayerAttributes = 0x4C0;       // ALTERADO (era 0x404) - LocalPlayerAttributes na V7A
    static inline uintptr_t NoReload2 = 0x91;               // mantido
    static inline uintptr_t GameTimer = 0x10;               // ALTERADO (era 0x24)

    static inline uintptr_t GameVariables = 0xb0;           // mantido

    static inline uintptr_t CurrentObserver = 0x64;         // mantido
    static inline uintptr_t ObserverPlayer = 0x28;          // mantido

    static inline uintptr_t RightcameraOffset = 0x38;       // mantido
    static inline uintptr_t FOVcameraoffset = 0x44;         // mantido
    static inline uintptr_t Backcameraoffset = 0x40;        // mantido
    static inline uintptr_t Upcameraoffset = 0x3c;          // mantido
    static inline uintptr_t Phase1CameraEulerAnglesY = 0x48;// mantido

    class Bones {
    public:
        static inline uintptr_t Head = 0x45C;   // ALTERADO (era 0x3F8)
        static inline uintptr_t Neck = 0x464;   // ALTERADO (era 0x400)  // Spine na V7A
        static inline uintptr_t LeftShoulder = 0x490;   // ALTERADO (era 0x42C)
        static inline uintptr_t RightShoulder = 0x494;   // ALTERADO (era 0x430)
        static inline uintptr_t LeftElbow = 0x4A4;   // ALTERADO (era 0x440)
        static inline uintptr_t RightElbow = 0x4A0;   // ALTERADO (era 0x43C)
        static inline uintptr_t LeftWrist = 0x49C;   // ALTERADO (era 0x3F4)
        static inline uintptr_t RightWrist = 0x498;   // ALTERADO (era 0x424)
        static inline uintptr_t Hip = 0x460;   // ALTERADO (era 0x3FC)
        static inline uintptr_t RightAnkle = 0x47C;   // ALTERADO (era 0x418)
        static inline uintptr_t LeftAnkle = 0x478;   // ALTERADO (era 0x414)
        static inline uintptr_t Root = 0x470;   // ALTERADO (era 0x40C)
    };
#endif
};