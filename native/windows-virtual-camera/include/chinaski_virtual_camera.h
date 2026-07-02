#pragma once

#ifdef _WIN32
#define CHINASKI_API extern "C" __declspec(dllexport)
#else
#define CHINASKI_API extern "C"
#endif

CHINASKI_API int RegisterChinaskiVirtualCamera();
CHINASKI_API int UnregisterChinaskiVirtualCamera();
CHINASKI_API int CheckChinaskiVirtualCameraSupport();
CHINASKI_API const char* GetChinaskiVirtualCameraBackend();
CHINASKI_API const wchar_t* GetChinaskiVirtualCameraName();
