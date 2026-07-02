#include "chinaski_virtual_camera.h"

#ifdef _WIN32
#include <mfapi.h>
#include <mfidl.h>
#endif

namespace {
constexpr wchar_t kCameraName[] = L"Chinaski Virtual Camera";
}

int RegisterChinaskiVirtualCamera() {
#ifndef _WIN32
  return -1;
#else
  // SPIKE: The final implementation must provide a registered COM media source
  // CLSID and pass it to MFCreateVirtualCamera. The current file exists to lock
  // the native boundary and build/link dependencies early.
  //
  // Expected next step:
  //   wil::com_ptr<IMFVirtualCamera> camera;
  //   MFCreateVirtualCamera(MFVirtualCameraType_SoftwareCameraSource,
  //                         MFVirtualCameraLifetime_System,
  //                         MFVirtualCameraAccess_AllUsers,
  //                         kCameraName,
  //                         sourceSymbolicLink,
  //                         nullptr,
  //                         0,
  //                         &camera);
  (void)kCameraName;
  return HRESULT_CODE(E_NOTIMPL);
#endif
}

int UnregisterChinaskiVirtualCamera() {
#ifndef _WIN32
  return -1;
#else
  // TODO: Store/reopen IMFVirtualCamera and call Remove when registration exists.
  return HRESULT_CODE(E_NOTIMPL);
#endif
}
