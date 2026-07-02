#include "chinaski_directshow_filter.h"
#include "chinaski_directshow_guids.h"

#include <dshow.h>
#include <new>

namespace {
long g_objectCount = 0;
constexpr wchar_t kCameraName[] = L"Chinaski Virtual Camera";

class ChinaskiEnumPins final : public IEnumPins {
 public:
  explicit ChinaskiEnumPins(IPin* pin) : pin_(pin) {
    if (pin_) {
      pin_->AddRef();
    }
    InterlockedIncrement(&g_objectCount);
  }

  ~ChinaskiEnumPins() {
    if (pin_) {
      pin_->Release();
    }
    InterlockedDecrement(&g_objectCount);
  }

  STDMETHODIMP QueryInterface(REFIID riid, void** object) override {
    if (!object) {
      return E_POINTER;
    }
    *object = nullptr;
    if (riid == IID_IUnknown || riid == IID_IEnumPins) {
      *object = static_cast<IEnumPins*>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  STDMETHODIMP_(ULONG) AddRef() override {
    return InterlockedIncrement(&refs_);
  }

  STDMETHODIMP_(ULONG) Release() override {
    const ULONG refs = InterlockedDecrement(&refs_);
    if (refs == 0) {
      delete this;
    }
    return refs;
  }

  STDMETHODIMP Next(ULONG count, IPin** pins, ULONG* fetched) override {
    if (!pins) {
      return E_POINTER;
    }
    if (fetched) {
      *fetched = 0;
    }
    if (count == 0) {
      return S_OK;
    }
    if (index_ > 0 || !pin_) {
      return S_FALSE;
    }
    pins[0] = pin_;
    pins[0]->AddRef();
    index_ = 1;
    if (fetched) {
      *fetched = 1;
    }
    return count == 1 ? S_OK : S_FALSE;
  }

  STDMETHODIMP Skip(ULONG count) override {
    index_ += count;
    return index_ <= 1 ? S_OK : S_FALSE;
  }

  STDMETHODIMP Reset() override {
    index_ = 0;
    return S_OK;
  }

  STDMETHODIMP Clone(IEnumPins** clone) override {
    if (!clone) {
      return E_POINTER;
    }
    auto next = new (std::nothrow) ChinaskiEnumPins(pin_);
    if (!next) {
      return E_OUTOFMEMORY;
    }
    next->index_ = index_;
    *clone = next;
    return S_OK;
  }

 private:
  long refs_ = 1;
  ULONG index_ = 0;
  IPin* pin_ = nullptr;
};

class ChinaskiEnumMediaTypes final : public IEnumMediaTypes {
 public:
  ChinaskiEnumMediaTypes() {
    InterlockedIncrement(&g_objectCount);
  }

  ~ChinaskiEnumMediaTypes() {
    InterlockedDecrement(&g_objectCount);
  }

  STDMETHODIMP QueryInterface(REFIID riid, void** object) override {
    if (!object) {
      return E_POINTER;
    }
    *object = nullptr;
    if (riid == IID_IUnknown || riid == IID_IEnumMediaTypes) {
      *object = static_cast<IEnumMediaTypes*>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  STDMETHODIMP_(ULONG) AddRef() override {
    return InterlockedIncrement(&refs_);
  }

  STDMETHODIMP_(ULONG) Release() override {
    const ULONG refs = InterlockedDecrement(&refs_);
    if (refs == 0) {
      delete this;
    }
    return refs;
  }

  STDMETHODIMP Next(ULONG count, AM_MEDIA_TYPE** types, ULONG* fetched) override {
    if (!types) {
      return E_POINTER;
    }
    if (fetched) {
      *fetched = 0;
    }
    if (count == 0) {
      return S_OK;
    }
    if (index_ > 0) {
      return S_FALSE;
    }

    AM_MEDIA_TYPE* mediaType =
        static_cast<AM_MEDIA_TYPE*>(CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE)));
    if (!mediaType) {
      return E_OUTOFMEMORY;
    }
    ZeroMemory(mediaType, sizeof(*mediaType));
    mediaType->majortype = MEDIATYPE_Video;
    mediaType->subtype = MEDIASUBTYPE_RGB32;
    mediaType->bFixedSizeSamples = TRUE;
    mediaType->bTemporalCompression = FALSE;
    mediaType->lSampleSize = 1280 * 720 * 4;
    mediaType->formattype = FORMAT_VideoInfo;
    mediaType->cbFormat = sizeof(VIDEOINFOHEADER);
    mediaType->pbFormat =
        static_cast<BYTE*>(CoTaskMemAlloc(sizeof(VIDEOINFOHEADER)));
    if (!mediaType->pbFormat) {
      CoTaskMemFree(mediaType);
      return E_OUTOFMEMORY;
    }

    auto* vih = reinterpret_cast<VIDEOINFOHEADER*>(mediaType->pbFormat);
    ZeroMemory(vih, sizeof(*vih));
    vih->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    vih->bmiHeader.biWidth = 1280;
    vih->bmiHeader.biHeight = 720;
    vih->bmiHeader.biPlanes = 1;
    vih->bmiHeader.biBitCount = 32;
    vih->bmiHeader.biCompression = BI_RGB;
    vih->bmiHeader.biSizeImage = 1280 * 720 * 4;
    vih->AvgTimePerFrame = 333333;

    types[0] = mediaType;
    index_ = 1;
    if (fetched) {
      *fetched = 1;
    }
    return count == 1 ? S_OK : S_FALSE;
  }

  STDMETHODIMP Skip(ULONG count) override {
    index_ += count;
    return index_ <= 1 ? S_OK : S_FALSE;
  }

  STDMETHODIMP Reset() override {
    index_ = 0;
    return S_OK;
  }

  STDMETHODIMP Clone(IEnumMediaTypes** clone) override {
    if (!clone) {
      return E_POINTER;
    }
    auto next = new (std::nothrow) ChinaskiEnumMediaTypes();
    if (!next) {
      return E_OUTOFMEMORY;
    }
    next->index_ = index_;
    *clone = next;
    return S_OK;
  }

 private:
  long refs_ = 1;
  ULONG index_ = 0;
};

class ChinaskiSourceFilter;

class ChinaskiOutputPin final : public IPin {
 public:
  explicit ChinaskiOutputPin(ChinaskiSourceFilter* filter) : filter_(filter) {
    InterlockedIncrement(&g_objectCount);
  }

  ~ChinaskiOutputPin() {
    InterlockedDecrement(&g_objectCount);
  }

  STDMETHODIMP QueryInterface(REFIID riid, void** object) override;
  STDMETHODIMP_(ULONG) AddRef() override {
    return InterlockedIncrement(&refs_);
  }
  STDMETHODIMP_(ULONG) Release() override {
    const ULONG refs = InterlockedDecrement(&refs_);
    if (refs == 0) {
      delete this;
    }
    return refs;
  }

  STDMETHODIMP Connect(IPin*, const AM_MEDIA_TYPE*) override {
    return E_NOTIMPL;
  }
  STDMETHODIMP ReceiveConnection(IPin* connector, const AM_MEDIA_TYPE*) override {
    connected_ = connector;
    if (connected_) {
      connected_->AddRef();
    }
    return S_OK;
  }
  STDMETHODIMP Disconnect() override {
    if (connected_) {
      connected_->Release();
      connected_ = nullptr;
    }
    return S_OK;
  }
  STDMETHODIMP ConnectedTo(IPin** pin) override {
    if (!pin) {
      return E_POINTER;
    }
    *pin = connected_;
    if (*pin) {
      (*pin)->AddRef();
      return S_OK;
    }
    return VFW_E_NOT_CONNECTED;
  }
  STDMETHODIMP ConnectionMediaType(AM_MEDIA_TYPE*) override {
    return VFW_E_NOT_CONNECTED;
  }
  STDMETHODIMP QueryPinInfo(PIN_INFO* info) override;
  STDMETHODIMP QueryDirection(PIN_DIRECTION* direction) override {
    if (!direction) {
      return E_POINTER;
    }
    *direction = PINDIR_OUTPUT;
    return S_OK;
  }
  STDMETHODIMP QueryId(LPWSTR* id) override {
    if (!id) {
      return E_POINTER;
    }
    *id = static_cast<LPWSTR>(CoTaskMemAlloc(sizeof(wchar_t) * 7));
    if (!*id) {
      return E_OUTOFMEMORY;
    }
    wcscpy_s(*id, 7, L"Output");
    return S_OK;
  }
  STDMETHODIMP QueryAccept(const AM_MEDIA_TYPE* type) override {
    if (!type) {
      return E_POINTER;
    }
    return type->majortype == MEDIATYPE_Video &&
                   type->subtype == MEDIASUBTYPE_RGB32
               ? S_OK
               : S_FALSE;
  }
  STDMETHODIMP EnumMediaTypes(IEnumMediaTypes** enumTypes) override {
    if (!enumTypes) {
      return E_POINTER;
    }
    *enumTypes = new (std::nothrow) ChinaskiEnumMediaTypes();
    return *enumTypes ? S_OK : E_OUTOFMEMORY;
  }
  STDMETHODIMP QueryInternalConnections(IPin**, ULONG*) override {
    return E_NOTIMPL;
  }
  STDMETHODIMP EndOfStream() override {
    return S_OK;
  }
  STDMETHODIMP BeginFlush() override {
    return S_OK;
  }
  STDMETHODIMP EndFlush() override {
    return S_OK;
  }
  STDMETHODIMP NewSegment(REFERENCE_TIME, REFERENCE_TIME, double) override {
    return S_OK;
  }

 private:
  long refs_ = 1;
  ChinaskiSourceFilter* filter_ = nullptr;
  IPin* connected_ = nullptr;
};

class ChinaskiSourceFilter final : public IBaseFilter {
 public:
  ChinaskiSourceFilter() {
    pin_ = new (std::nothrow) ChinaskiOutputPin(this);
    InterlockedIncrement(&g_objectCount);
  }

  ~ChinaskiSourceFilter() {
    if (pin_) {
      pin_->Release();
    }
    InterlockedDecrement(&g_objectCount);
  }

  STDMETHODIMP QueryInterface(REFIID riid, void** object) override {
    if (!object) {
      return E_POINTER;
    }
    *object = nullptr;
    if (riid == IID_IUnknown || riid == IID_IPersist ||
        riid == IID_IMediaFilter || riid == IID_IBaseFilter) {
      *object = static_cast<IBaseFilter*>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  STDMETHODIMP_(ULONG) AddRef() override {
    return InterlockedIncrement(&refs_);
  }
  STDMETHODIMP_(ULONG) Release() override {
    const ULONG refs = InterlockedDecrement(&refs_);
    if (refs == 0) {
      delete this;
    }
    return refs;
  }

  STDMETHODIMP GetClassID(CLSID* classId) override {
    if (!classId) {
      return E_POINTER;
    }
    *classId = CLSID_ChinaskiDirectShowVirtualCamera;
    return S_OK;
  }
  STDMETHODIMP Stop() override {
    state_ = State_Stopped;
    return S_OK;
  }
  STDMETHODIMP Pause() override {
    state_ = State_Paused;
    return S_OK;
  }
  STDMETHODIMP Run(REFERENCE_TIME start) override {
    start_ = start;
    state_ = State_Running;
    return S_OK;
  }
  STDMETHODIMP GetState(DWORD, FILTER_STATE* state) override {
    if (!state) {
      return E_POINTER;
    }
    *state = state_;
    return S_OK;
  }
  STDMETHODIMP SetSyncSource(IReferenceClock* clock) override {
    if (clock) {
      clock->AddRef();
    }
    if (clock_) {
      clock_->Release();
    }
    clock_ = clock;
    return S_OK;
  }
  STDMETHODIMP GetSyncSource(IReferenceClock** clock) override {
    if (!clock) {
      return E_POINTER;
    }
    *clock = clock_;
    if (*clock) {
      (*clock)->AddRef();
    }
    return S_OK;
  }
  STDMETHODIMP EnumPins(IEnumPins** enumPins) override {
    if (!enumPins) {
      return E_POINTER;
    }
    *enumPins = new (std::nothrow) ChinaskiEnumPins(pin_);
    return *enumPins ? S_OK : E_OUTOFMEMORY;
  }
  STDMETHODIMP FindPin(LPCWSTR id, IPin** pin) override {
    if (!pin) {
      return E_POINTER;
    }
    *pin = nullptr;
    if (id && _wcsicmp(id, L"Output") == 0 && pin_) {
      *pin = pin_;
      (*pin)->AddRef();
      return S_OK;
    }
    return VFW_E_NOT_FOUND;
  }
  STDMETHODIMP QueryFilterInfo(FILTER_INFO* info) override {
    if (!info) {
      return E_POINTER;
    }
    wcscpy_s(info->achName, kCameraName);
    info->pGraph = nullptr;
    return S_OK;
  }
  STDMETHODIMP JoinFilterGraph(IFilterGraph* graph, LPCWSTR) override {
    graph_ = graph;
    return S_OK;
  }
  STDMETHODIMP QueryVendorInfo(LPWSTR* vendorInfo) override {
    if (!vendorInfo) {
      return E_POINTER;
    }
    *vendorInfo = static_cast<LPWSTR>(CoTaskMemAlloc(sizeof(wchar_t) * 9));
    if (!*vendorInfo) {
      return E_OUTOFMEMORY;
    }
    wcscpy_s(*vendorInfo, 9, L"Chinaski");
    return S_OK;
  }

 private:
  long refs_ = 1;
  FILTER_STATE state_ = State_Stopped;
  REFERENCE_TIME start_ = 0;
  ChinaskiOutputPin* pin_ = nullptr;
  IReferenceClock* clock_ = nullptr;
  IFilterGraph* graph_ = nullptr;

  friend class ChinaskiOutputPin;
};

STDMETHODIMP ChinaskiOutputPin::QueryInterface(REFIID riid, void** object) {
  if (!object) {
    return E_POINTER;
  }
  *object = nullptr;
  if (riid == IID_IUnknown || riid == IID_IPin) {
    *object = static_cast<IPin*>(this);
    AddRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}

STDMETHODIMP ChinaskiOutputPin::QueryPinInfo(PIN_INFO* info) {
  if (!info) {
    return E_POINTER;
  }
  info->dir = PINDIR_OUTPUT;
  wcscpy_s(info->achName, L"Output");
  info->pFilter = filter_;
  if (info->pFilter) {
    info->pFilter->AddRef();
  }
  return S_OK;
}

class ChinaskiClassFactory final : public IClassFactory {
 public:
  ChinaskiClassFactory() {
    InterlockedIncrement(&g_objectCount);
  }
  ~ChinaskiClassFactory() {
    InterlockedDecrement(&g_objectCount);
  }

  STDMETHODIMP QueryInterface(REFIID riid, void** object) override {
    if (!object) {
      return E_POINTER;
    }
    *object = nullptr;
    if (riid == IID_IUnknown || riid == IID_IClassFactory) {
      *object = static_cast<IClassFactory*>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }
  STDMETHODIMP_(ULONG) AddRef() override {
    return InterlockedIncrement(&refs_);
  }
  STDMETHODIMP_(ULONG) Release() override {
    const ULONG refs = InterlockedDecrement(&refs_);
    if (refs == 0) {
      delete this;
    }
    return refs;
  }
  STDMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** object) override {
    if (outer) {
      return CLASS_E_NOAGGREGATION;
    }
    return CreateChinaskiDirectShowFilter(riid, object);
  }
  STDMETHODIMP LockServer(BOOL lock) override {
    if (lock) {
      InterlockedIncrement(&g_objectCount);
    } else {
      InterlockedDecrement(&g_objectCount);
    }
    return S_OK;
  }

 private:
  long refs_ = 1;
};
}

HRESULT CreateChinaskiDirectShowFilter(REFIID riid, void** object) {
  if (!object) {
    return E_POINTER;
  }
  *object = nullptr;
  auto filter = new (std::nothrow) ChinaskiSourceFilter();
  if (!filter) {
    return E_OUTOFMEMORY;
  }
  HRESULT hr = filter->QueryInterface(riid, object);
  filter->Release();
  return hr;
}

extern "C" HRESULT __stdcall DllGetClassObject(REFCLSID classId, REFIID riid,
                                               void** object) {
  if (classId != CLSID_ChinaskiDirectShowVirtualCamera) {
    return CLASS_E_CLASSNOTAVAILABLE;
  }
  auto factory = new (std::nothrow) ChinaskiClassFactory();
  if (!factory) {
    return E_OUTOFMEMORY;
  }
  HRESULT hr = factory->QueryInterface(riid, object);
  factory->Release();
  return hr;
}

extern "C" HRESULT __stdcall DllCanUnloadNow() {
  return g_objectCount == 0 ? S_OK : S_FALSE;
}

extern "C" HRESULT __stdcall DllRegisterServer() {
  wchar_t modulePath[MAX_PATH]{};
  HMODULE module = nullptr;
  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                              GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCWSTR>(&DllRegisterServer),
                          &module)) {
    return HRESULT_FROM_WIN32(GetLastError());
  }
  if (!GetModuleFileNameW(module, modulePath, ARRAYSIZE(modulePath))) {
    return HRESULT_FROM_WIN32(GetLastError());
  }
  return RegisterChinaskiDirectShowFilter(modulePath);
}

extern "C" HRESULT __stdcall DllUnregisterServer() {
  return UnregisterChinaskiDirectShowFilter();
}
