#pragma once
#include <intrin.h>

static bool OpaqueTrue() {
    volatile int a = 42, b = 7;
    return (a * b) % 3 == 0;
}

static bool OpaqueFalse() {
    volatile int a = 42, b = 7;
    return (a * b) % 7 == 0;
}

static void JunkCode() {
    volatile unsigned __int64 x = __rdtsc();
    for (volatile int i = 0; i < 5; i++) x ^= (x << 3) + i;
    if (x == 0xFFFFFFFFFFFFFFFF) __nop();
}

#define JUNK() do { if (OpaqueTrue()) JunkCode(); } while(0)
#define JUNK_FALSE() do { if (OpaqueFalse()) { volatile unsigned __int64 _x = __rdtsc(); _x = _x * _x; } } while(0)

static void AntiDebugCheck() {
    JUNK();
    BOOL isDebugger = FALSE;
    CheckRemoteDebuggerPresent(GetCurrentProcess(), &isDebugger);
    if (isDebugger) {
        volatile int x = 0;
        for (volatile int i = 0; i < 50; i++) x += i;
        if (x == 1225) ExitProcess(0);
    }
    JUNK_FALSE();
}
