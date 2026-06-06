#pragma once
#include <string.h>

#define C_OBF_DECL(nm, len, key) \
    static unsigned char _cob_##nm[len] = {
#define C_OBF_END(nm, len, key) \
    }; \
    static char* cob_##nm(void) { \
        static int _init = 0; \
        if (!_init) { \
            for (int _i = 0; _i < len; _i++) \
                _cob_##nm[_i] ^= (unsigned char)(key + _i); \
            _init = 1; \
        } \
        return (char*)_cob_##nm; \
    }

#define C_OBF_DECLW(nm, len, key) \
    static unsigned short _cobw_##nm[len] = {
#define C_OBF_ENDW(nm, len, key) \
    }; \
    static wchar_t* cobw_##nm(void) { \
        static int _init = 0; \
        if (!_init) { \
            for (int _i = 0; _i < len; _i++) \
                _cobw_##nm[_i] ^= (unsigned short)(key + _i); \
            _init = 1; \
        } \
        return (wchar_t*)_cobw_##nm; \
    }
