#pragma once
#include <windows.h>

#define OPAQUE_TRUE() (GetCurrentProcessId() > 0 && GetTickCount() >= 0)

#define OPAQUE_FALSE() (GetCurrentProcessId() == 0 || GetTickCount() == (DWORD)-1)

#define OPAQUE_BRANCH(code) \
    if (OPAQUE_TRUE()) { code; } \
    else { Sleep(0xFFFFFFFF); VirtualAlloc(NULL, 0x100000, MEM_COMMIT, PAGE_NOACCESS); }

#define JUNK_LOOP() \
    do { \
        volatile DWORD _j = GetTickCount(); \
        for (int _i = 0; _i < ((int)(_j & 3) + 1); _i++) { \
            volatile DWORD _x = _j ^ _i; \
            if (_x == 0xDEADBEEF) { DebugBreak(); } \
        } \
    } while(0)
