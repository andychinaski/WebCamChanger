#include "chinaski_directshow_filter.h"
#include "chinaski_directshow_guids.h"

#include <dshow.h>
#include <strsafe.h>

namespace {
constexpr wchar_t kCameraName[] = L"Chinaski Virtual Camera";
constexpr wchar_t kThreadingModel[] = L"Both";

class ComApartment {
 public:
  ComApartment() : hr_(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) {
    if (hr_ == RPC_E_CHANGED_MODE) {
      hr_ = S_OK;
      shouldUninitialize_ = false;
    } else {
      shouldUninitialize_ = SUCCEEDED(hr_);
    }
  }

  ~ComApartment() {
    if (shouldUninitialize_) {
      CoUninitialize();
    }
  }

  HRESULT hr() const { return hr_; }

 private:
  HRESULT hr_;
  bool shouldUninitialize_ = false;
};

HRESULT SetStringValue(HKEY key, const wchar_t* name, const wchar_t* value) {
  const DWORD bytes = static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t));
  return HRESULT_FROM_WIN32(RegSetValueExW(
      key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value), bytes));
}

HRESULT CreateKey(HKEY root, const wchar_t* path, HKEY* key) {
  DWORD disposition = 0;
  return HRESULT_FROM_WIN32(RegCreateKeyExW(
      root, path, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, key,
      &disposition));
}

HRESULT GuidToString(REFGUID guid, wchar_t* text, DWORD textLength) {
  const int written = StringFromGUID2(guid, text, static_cast<int>(textLength));
  return written > 0 ? S_OK : E_FAIL;
}

HRESULT RegisterComServerInRoot(HKEY root, const wchar_t* modulePath) {
  wchar_t clsidText[64]{};
  HRESULT hr = GuidToString(CLSID_ChinaskiDirectShowVirtualCamera, clsidText,
                            ARRAYSIZE(clsidText));
  if (FAILED(hr)) {
    return hr;
  }

  wchar_t clsidPath[160]{};
  hr = StringCchPrintfW(clsidPath, ARRAYSIZE(clsidPath),
                        L"Software\\Classes\\CLSID\\%s", clsidText);
  if (FAILED(hr)) {
    return hr;
  }

  HKEY clsidKey = nullptr;
  hr = CreateKey(root, clsidPath, &clsidKey);
  if (FAILED(hr)) {
    return hr;
  }

  hr = SetStringValue(clsidKey, nullptr, kCameraName);
  RegCloseKey(clsidKey);
  if (FAILED(hr)) {
    return hr;
  }

  wchar_t inprocPath[192]{};
  hr = StringCchPrintfW(inprocPath, ARRAYSIZE(inprocPath), L"%s\\InprocServer32",
                        clsidPath);
  if (FAILED(hr)) {
    return hr;
  }

  HKEY inprocKey = nullptr;
  hr = CreateKey(root, inprocPath, &inprocKey);
  if (FAILED(hr)) {
    return hr;
  }

  hr = SetStringValue(inprocKey, nullptr, modulePath);
  if (SUCCEEDED(hr)) {
    hr = SetStringValue(inprocKey, L"ThreadingModel", kThreadingModel);
  }
  RegCloseKey(inprocKey);
  return hr;
}

HRESULT RegisterComServer(const wchar_t* modulePath) {
  HRESULT hr = RegisterComServerInRoot(HKEY_CURRENT_USER, modulePath);
  if (FAILED(hr)) {
    return hr;
  }

  const HRESULT machineHr = RegisterComServerInRoot(HKEY_LOCAL_MACHINE, modulePath);
  if (machineHr == HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED)) {
    return S_OK;
  }
  return machineHr;
}

HRESULT UnregisterComServerInRoot(HKEY root) {
  wchar_t clsidText[64]{};
  HRESULT hr = GuidToString(CLSID_ChinaskiDirectShowVirtualCamera, clsidText,
                            ARRAYSIZE(clsidText));
  if (FAILED(hr)) {
    return hr;
  }

  wchar_t clsidPath[160]{};
  hr = StringCchPrintfW(clsidPath, ARRAYSIZE(clsidPath),
                        L"Software\\Classes\\CLSID\\%s", clsidText);
  if (FAILED(hr)) {
    return hr;
  }

  const LSTATUS status = RegDeleteTreeW(root, clsidPath);
  if (status == ERROR_FILE_NOT_FOUND) {
    return S_OK;
  }
  return HRESULT_FROM_WIN32(status);
}

HRESULT UnregisterComServer() {
  const HRESULT userHr = UnregisterComServerInRoot(HKEY_CURRENT_USER);
  const HRESULT machineHr = UnregisterComServerInRoot(HKEY_LOCAL_MACHINE);
  if (FAILED(userHr)) {
    return userHr;
  }
  if (machineHr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) ||
      machineHr == HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED)) {
    return S_OK;
  }
  return machineHr;
}

HRESULT RegisterWithFilterMapper() {
  ComApartment com;
  if (FAILED(com.hr())) {
    return com.hr();
  }

  IFilterMapper2* mapper = nullptr;
  HRESULT hr = CoCreateInstance(CLSID_FilterMapper2, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&mapper));
  if (FAILED(hr)) {
    return hr;
  }

  REGPINTYPES pinTypes{};
  pinTypes.clsMajorType = &MEDIATYPE_Video;
  pinTypes.clsMinorType = &MEDIASUBTYPE_RGB32;

  REGFILTERPINS2 pin{};
  pin.dwFlags = REG_PINFLAG_B_OUTPUT;
  pin.nMediaTypes = 1;
  pin.lpMediaType = &pinTypes;
  pin.nMediums = 0;
  pin.lpMedium = nullptr;
  pin.clsPinCategory = &PIN_CATEGORY_CAPTURE;

  REGFILTER2 filter{};
  filter.dwVersion = 2;
  filter.dwMerit = MERIT_DO_NOT_USE + 1;
  filter.cPins2 = 1;
  filter.rgPins2 = &pin;

  hr = mapper->RegisterFilter(CLSID_ChinaskiDirectShowVirtualCamera, kCameraName,
                              nullptr, &CLSID_VideoInputDeviceCategory,
                              kCameraName, &filter);
  mapper->Release();
  return hr;
}

HRESULT UnregisterWithFilterMapper() {
  ComApartment com;
  if (FAILED(com.hr())) {
    return com.hr();
  }

  IFilterMapper2* mapper = nullptr;
  HRESULT hr = CoCreateInstance(CLSID_FilterMapper2, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&mapper));
  if (FAILED(hr)) {
    return hr;
  }

  hr = mapper->UnregisterFilter(&CLSID_VideoInputDeviceCategory, kCameraName,
                                CLSID_ChinaskiDirectShowVirtualCamera);
  mapper->Release();
  return hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) ? S_OK : hr;
}
}

HRESULT RegisterChinaskiDirectShowFilter(const wchar_t* modulePath) {
  if (!modulePath || modulePath[0] == L'\0') {
    return E_INVALIDARG;
  }

  HRESULT hr = RegisterComServer(modulePath);
  if (FAILED(hr)) {
    return hr;
  }

  hr = RegisterWithFilterMapper();
  if (FAILED(hr)) {
    UnregisterComServer();
  }
  return hr;
}

HRESULT UnregisterChinaskiDirectShowFilter() {
  HRESULT mapperHr = UnregisterWithFilterMapper();
  HRESULT comHr = UnregisterComServer();
  return FAILED(mapperHr) ? mapperHr : comHr;
}
