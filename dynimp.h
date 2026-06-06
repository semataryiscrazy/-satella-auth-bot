#pragma once
#include <windows.h>

template<typename T>
inline T _R(const char* m, const char* f) {
    HMODULE h = GetModuleHandleA(m);
    if (!h) { LoadLibraryA(m); h = GetModuleHandleA(m); }
    return h ? (T)GetProcAddress(h, f) : nullptr;
}

#define RESOLVE(mod, func) _R<decltype(&func)>(mod, func)
#define K32(func) _R<decltype(&func)>("kernel32.dll", #func)
#define A32(func) _R<decltype(&func)>("advapi32.dll", #func)
#define S32(func) _R<decltype(&func)>("shell32.dll", #func)
#define NTD(func) _R<decltype(&func)>("ntdll.dll", #func)

#define INIT(fn) static auto _##fn = (decltype(&fn))0; if (!_##fn) _##fn = K32(fn)
#define INIT_A(fn) static auto _##fn = (decltype(&fn))0; if (!_##fn) _##fn = A32(fn)
#define INIT_S(fn) static auto _##fn = (decltype(&fn))0; if (!_##fn) _##fn = S32(fn)
#define INIT_N(fn) static auto _##fn = (decltype(&fn))0; if (!_##fn) _##fn = NTD(fn)
#define CALL(fn) _##fn
