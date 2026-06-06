#pragma once

#ifdef LOADERDLL_EXPORTS
#define LOADERDLL_API __declspec(dllexport)
#else
#define LOADERDLL_API __declspec(dllimport)
#endif

// Pure C++ interface - no Windows.h here
LOADERDLL_API void StartLoader();
LOADERDLL_API void StopLoader();
