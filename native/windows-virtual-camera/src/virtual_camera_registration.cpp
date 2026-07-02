#include "chinaski_virtual_camera.h"
#include "chinaski_directshow_filter.h"

#ifdef _WIN32
#include <combaseapi.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfvirtualcamera.h>
#include <strsafe.h>
#include <windows.h>
#endif

namespace {
constexpr wchar_t kCameraName[] = L"Chinaski Virtual Camera";
constexpr DWORD kWindows10Build = 10240;
constexpr DWORD kWindows11Build = 22000;

#ifdef _WIN32
using MFIsVirtualCameraTypeSupportedFn = HRESULT(WINAPI*)(
    MFVirtualCameraType type,
    BOOL* supported);

class MfPlatformScope {
 public:
  MfPlatformScope() : hr_(MFStartup(MF_VERSION, MFSTARTUP_FULL)) {}
  ~MfPlatformScope() {
    if (SUCCEEDED(hr_)) {
      MFShutdown();
    }
  }

  HRESULT hr() const { return hr_; }

 private:
  HRESULT hr_;
};

DWORD GetWindowsBuildNumber() {
  using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);

  HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  if (!ntdll) {
    return 0;
  }

  auto rtlGetVersion = reinterpret_cast<RtlGetVersionFn>(
      GetProcAddress(ntdll, "RtlGetVersion"));
  if (!rtlGetVersion) {
    return 0;
  }

  RTL_OSVERSIONINFOW version{};
  version.dwOSVersionInfoSize = sizeof(version);
  if (rtlGetVersion(&version) != 0) {
    return 0;
  }

  return version.dwBuildNumber;
}

bool IsWindows10OrNewer() {
  return GetWindowsBuildNumber() >= kWindows10Build;
}

bool IsWindows11OrNewer() {
  return GetWindowsBuildNumber() >= kWindows11Build;
}

HRESULT IsVirtualCameraTypeSupportedDynamic(BOOL* supported) {
  if (!supported) {
    return E_POINTER;
  }

  HMODULE sensorGroup = LoadLibraryW(L"mfsensorgroup.dll");
  if (!sensorGroup) {
    return HRESULT_FROM_WIN32(GetLastError());
  }

  auto fn = reinterpret_cast<MFIsVirtualCameraTypeSupportedFn>(
      GetProcAddress(sensorGroup, "MFIsVirtualCameraTypeSupported"));
  if (!fn) {
    const DWORD error = GetLastError();
    FreeLibrary(sensorGroup);
    return error == ERROR_PROC_NOT_FOUND
               ? HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED)
               : HRESULT_FROM_WIN32(error);
  }

  const HRESULT hr = fn(MFVirtualCameraType_SoftwareCameraSource, supported);
  FreeLibrary(sensorGroup);
  return hr;
}

HRESULT CheckDirectShowVirtualCameraSupport() {
  if (!IsWindows10OrNewer()) {
    return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
  }

  // DirectShow is the Windows 10 fallback backend. It needs a COM source filter
  // registered under CLSID_VideoInputDeviceCategory. The runtime check here only
  // verifies that the DirectShow runtime is present; actual registration is the
  // next native task.
  HMODULE quartz = LoadLibraryW(L"quartz.dll");
  if (!quartz) {
    return HRESULT_FROM_WIN32(GetLastError());
  }

  FreeLibrary(quartz);
  return S_OK;
}

HRESULT CheckMediaFoundationVirtualCameraSupport() {
  if (!IsWindows11OrNewer()) {
    return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
  }

  MfPlatformScope mf;
  if (FAILED(mf.hr())) {
    return mf.hr();
  }

  BOOL supported = FALSE;
  HRESULT hr = IsVirtualCameraTypeSupportedDynamic(&supported);
  if (FAILED(hr)) {
    return hr;
  }

  return supported ? S_OK : HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
}

HRESULT GetSiblingVirtualCameraDllPath(wchar_t* path, DWORD pathLength) {
  if (!path || pathLength == 0) {
    return E_INVALIDARG;
  }

  wchar_t modulePath[MAX_PATH]{};
  DWORD size = GetModuleFileNameW(nullptr, modulePath, ARRAYSIZE(modulePath));
  if (size == 0 || size >= ARRAYSIZE(modulePath)) {
    return HRESULT_FROM_WIN32(GetLastError());
  }

  wchar_t* lastSlash = wcsrchr(modulePath, L'\\');
  if (!lastSlash) {
    return E_FAIL;
  }
  *(lastSlash + 1) = L'\0';

  HRESULT hr = StringCchCopyW(path, pathLength, modulePath);
  if (FAILED(hr)) {
    return hr;
  }
  return StringCchCatW(path, pathLength, L"ChinaskiVirtualCamera.dll");
}
#endif
}

const wchar_t* GetChinaskiVirtualCameraName() {
  return kCameraName;
}

const char* GetChinaskiVirtualCameraBackend() {
#ifndef _WIN32
  return "unsupported";
#else
  if (SUCCEEDED(CheckMediaFoundationVirtualCameraSupport())) {
    return "media-foundation";
  }
  if (SUCCEEDED(CheckDirectShowVirtualCameraSupport())) {
    return "directshow";
  }
  return "unsupported";
#endif
}

int CheckChinaskiVirtualCameraSupport() {
#ifndef _WIN32
  return -1;
#else
  HRESULT mfHr = CheckMediaFoundationVirtualCameraSupport();
  if (SUCCEEDED(mfHr)) {
    return 0;
  }

  HRESULT dshowHr = CheckDirectShowVirtualCameraSupport();
  if (SUCCEEDED(dshowHr)) {
    return 0;
  }

  return static_cast<int>(mfHr != HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED) ? mfHr : dshowHr);
#endif
}

int RegisterChinaskiVirtualCamera() {
#ifndef _WIN32
  return -1;
#else
  if (SUCCEEDED(CheckMediaFoundationVirtualCameraSupport())) {
    // SPIKE: MFCreateVirtualCamera requires sourceId to point at a registered
    // Custom Media Source. We intentionally return E_NOTIMPL here until that COM
    // source exists, because registering a device without a frame-producing
    // source would only create a misleading half-success.
    //
    // Expected final shape:
    //   IMFVirtualCamera* camera = nullptr;
    //   HRESULT hr = MFCreateVirtualCamera(
    //       MFVirtualCameraType_SoftwareCameraSource,
    //       MFVirtualCameraLifetime_System,
    //       MFVirtualCameraAccess_AllUsers,
    //       kCameraName,
    //       registeredCustomMediaSourceId,
    //       nullptr,
    //       0,
    //       &camera);
    (void)kCameraName;
    return static_cast<int>(E_NOTIMPL);
  }

  HRESULT dshowSupport = CheckDirectShowVirtualCameraSupport();
  if (FAILED(dshowSupport)) {
    return static_cast<int>(dshowSupport);
  }

  wchar_t dllPath[MAX_PATH]{};
  HRESULT pathHr = GetSiblingVirtualCameraDllPath(dllPath, ARRAYSIZE(dllPath));
  if (FAILED(pathHr)) {
    return static_cast<int>(pathHr);
  }

  return static_cast<int>(RegisterChinaskiDirectShowFilter(dllPath));
#endif
}

int UnregisterChinaskiVirtualCamera() {
#ifndef _WIN32
  return -1;
#else
  if (SUCCEEDED(CheckMediaFoundationVirtualCameraSupport())) {
    // TODO: Store/reopen IMFVirtualCamera and call Remove when registration exists.
    return static_cast<int>(E_NOTIMPL);
  }

  HRESULT dshowSupport = CheckDirectShowVirtualCameraSupport();
  if (FAILED(dshowSupport)) {
    return static_cast<int>(dshowSupport);
  }

  return static_cast<int>(UnregisterChinaskiDirectShowFilter());
#endif
}
