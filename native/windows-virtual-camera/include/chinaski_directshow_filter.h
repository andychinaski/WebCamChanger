#pragma once

#include <windows.h>

HRESULT RegisterChinaskiDirectShowFilter(const wchar_t* modulePath);
HRESULT UnregisterChinaskiDirectShowFilter();
HRESULT CreateChinaskiDirectShowFilter(REFIID riid, void** object);

