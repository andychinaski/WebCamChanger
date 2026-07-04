#include "chinaski_directshow_filter.h"
#include "chinaski_directshow_guids.h"

#include <dshow.h>
#include <new>
#include <stdio.h>
#include <strsafe.h>

namespace {
long g_objectCount = 0;
constexpr wchar_t kCameraName[] = L"Chinaski Virtual Camera";
constexpr LONG kFrameWidth = 1280;
constexpr LONG kFrameHeight = 720;
constexpr LONG kFrameBytesPerPixel = 4;
constexpr LONG kFrameSize = kFrameWidth * kFrameHeight * kFrameBytesPerPixel;
constexpr REFERENCE_TIME kFrameDuration = 333333;

void Log(const char* message) {
  wchar_t tempPath[MAX_PATH]{};
  if (!GetTempPathW(ARRAYSIZE(tempPath), tempPath)) {
    return;
  }

  wchar_t logPath[MAX_PATH]{};
  if (FAILED(StringCchPrintfW(logPath, ARRAYSIZE(logPath), L"%schinaski-vcam.log",
                              tempPath))) {
    return;
  }

  HANDLE file = CreateFileW(logPath, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    return;
  }

  SYSTEMTIME now{};
  GetLocalTime(&now);
  char line[512]{};
  int length = sprintf_s(line, "[%02u:%02u:%02u.%03u] %s\r\n", now.wHour,
                         now.wMinute, now.wSecond, now.wMilliseconds, message);
  if (length > 0) {
    DWORD written = 0;
    WriteFile(file, line, static_cast<DWORD>(length), &written, nullptr);
  }
  CloseHandle(file);
}

void FreeMediaType(AM_MEDIA_TYPE& mediaType) {
  if (mediaType.cbFormat != 0) {
    CoTaskMemFree(mediaType.pbFormat);
    mediaType.cbFormat = 0;
    mediaType.pbFormat = nullptr;
  }
  if (mediaType.pUnk) {
    mediaType.pUnk->Release();
    mediaType.pUnk = nullptr;
  }
}

HRESULT CopyMediaType(AM_MEDIA_TYPE* destination, const AM_MEDIA_TYPE* source) {
  if (!destination || !source) {
    return E_POINTER;
  }

  *destination = *source;
  if (source->cbFormat != 0) {
    destination->pbFormat = static_cast<BYTE*>(CoTaskMemAlloc(source->cbFormat));
    if (!destination->pbFormat) {
      destination->cbFormat = 0;
      return E_OUTOFMEMORY;
    }
    CopyMemory(destination->pbFormat, source->pbFormat, source->cbFormat);
  }
  if (destination->pUnk) {
    destination->pUnk->AddRef();
  }
  return S_OK;
}

void FillVideoStreamCaps(VIDEO_STREAM_CONFIG_CAPS* caps) {
  ZeroMemory(caps, sizeof(*caps));
  caps->guid = FORMAT_VideoInfo;
  caps->VideoStandard = AnalogVideo_None;
  caps->InputSize.cx = kFrameWidth;
  caps->InputSize.cy = kFrameHeight;
  caps->MinCroppingSize.cx = kFrameWidth;
  caps->MinCroppingSize.cy = kFrameHeight;
  caps->MaxCroppingSize.cx = kFrameWidth;
  caps->MaxCroppingSize.cy = kFrameHeight;
  caps->CropGranularityX = 1;
  caps->CropGranularityY = 1;
  caps->CropAlignX = 1;
  caps->CropAlignY = 1;
  caps->MinOutputSize.cx = kFrameWidth;
  caps->MinOutputSize.cy = kFrameHeight;
  caps->MaxOutputSize.cx = kFrameWidth;
  caps->MaxOutputSize.cy = kFrameHeight;
  caps->OutputGranularityX = 1;
  caps->OutputGranularityY = 1;
  caps->MinFrameInterval = kFrameDuration;
  caps->MaxFrameInterval = kFrameDuration;
  caps->MinBitsPerSecond = kFrameSize * 8 * 30;
  caps->MaxBitsPerSecond = kFrameSize * 8 * 30;
}

HRESULT CreateRgb32MediaType(AM_MEDIA_TYPE* mediaType) {
  if (!mediaType) {
    return E_POINTER;
  }

  ZeroMemory(mediaType, sizeof(*mediaType));
  mediaType->majortype = MEDIATYPE_Video;
  mediaType->subtype = MEDIASUBTYPE_RGB32;
  mediaType->bFixedSizeSamples = TRUE;
  mediaType->bTemporalCompression = FALSE;
  mediaType->lSampleSize = kFrameSize;
  mediaType->formattype = FORMAT_VideoInfo;
  mediaType->cbFormat = sizeof(VIDEOINFOHEADER);
  mediaType->pbFormat =
      static_cast<BYTE*>(CoTaskMemAlloc(sizeof(VIDEOINFOHEADER)));
  if (!mediaType->pbFormat) {
    mediaType->cbFormat = 0;
    return E_OUTOFMEMORY;
  }

  auto* vih = reinterpret_cast<VIDEOINFOHEADER*>(mediaType->pbFormat);
  ZeroMemory(vih, sizeof(*vih));
  vih->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  vih->bmiHeader.biWidth = kFrameWidth;
  vih->bmiHeader.biHeight = kFrameHeight;
  vih->bmiHeader.biPlanes = 1;
  vih->bmiHeader.biBitCount = 32;
  vih->bmiHeader.biCompression = BI_RGB;
  vih->bmiHeader.biSizeImage = kFrameSize;
  vih->AvgTimePerFrame = kFrameDuration;
  return S_OK;
}

bool IsSupportedMediaType(const AM_MEDIA_TYPE* type) {
  if (!type) {
    return false;
  }
  if (type->majortype != MEDIATYPE_Video || type->subtype != MEDIASUBTYPE_RGB32) {
    return false;
  }
  if (type->formattype != FORMAT_VideoInfo || !type->pbFormat ||
      type->cbFormat < sizeof(VIDEOINFOHEADER)) {
    return true;
  }

  auto* vih = reinterpret_cast<VIDEOINFOHEADER*>(type->pbFormat);
  return vih->bmiHeader.biWidth == kFrameWidth &&
         labs(vih->bmiHeader.biHeight) == kFrameHeight &&
         vih->bmiHeader.biBitCount == 32;
}

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
    mediaType->lSampleSize = kFrameSize;
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
    vih->bmiHeader.biWidth = kFrameWidth;
    vih->bmiHeader.biHeight = kFrameHeight;
    vih->bmiHeader.biPlanes = 1;
    vih->bmiHeader.biBitCount = 32;
    vih->bmiHeader.biCompression = BI_RGB;
    vih->bmiHeader.biSizeImage = kFrameSize;
    vih->AvgTimePerFrame = kFrameDuration;

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

class ChinaskiOutputPin final : public IPin, public IAMStreamConfig {
 public:
  explicit ChinaskiOutputPin(ChinaskiSourceFilter* filter) : filter_(filter) {
    InitializeCriticalSection(&lock_);
    stopEvent_ = CreateEventW(nullptr, TRUE, TRUE, nullptr);
    InterlockedIncrement(&g_objectCount);
  }

  ~ChinaskiOutputPin() {
    StopStreaming();
    Disconnect();
    if (hasPreferredType_) {
      FreeMediaType(preferredType_);
      hasPreferredType_ = false;
    }
    if (stopEvent_) {
      CloseHandle(stopEvent_);
      stopEvent_ = nullptr;
    }
    DeleteCriticalSection(&lock_);
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

  STDMETHODIMP Connect(IPin* receivePin, const AM_MEDIA_TYPE* mediaType) override {
    if (!receivePin) {
      return E_POINTER;
    }

    EnterCriticalSection(&lock_);
    const bool alreadyConnected = connected_ != nullptr;
    LeaveCriticalSection(&lock_);
    if (alreadyConnected) {
      return VFW_E_ALREADY_CONNECTED;
    }

    PIN_DIRECTION direction = PINDIR_OUTPUT;
    HRESULT hr = receivePin->QueryDirection(&direction);
    if (FAILED(hr)) {
      return hr;
    }
    if (direction != PINDIR_INPUT) {
      return VFW_E_INVALID_DIRECTION;
    }

    AM_MEDIA_TYPE selected{};
    if (mediaType) {
      if (!IsSupportedMediaType(mediaType)) {
        return VFW_E_TYPE_NOT_ACCEPTED;
      }
      hr = CopyMediaType(&selected, mediaType);
    } else if (hasPreferredType_) {
      hr = CopyMediaType(&selected, &preferredType_);
    } else {
      hr = CreateRgb32MediaType(&selected);
    }
    if (FAILED(hr)) {
      return hr;
    }

    hr = receivePin->ReceiveConnection(this, &selected);
    if (FAILED(hr)) {
      FreeMediaType(selected);
      return hr;
    }

    IMemInputPin* input = nullptr;
    hr = receivePin->QueryInterface(IID_PPV_ARGS(&input));
    if (FAILED(hr)) {
      receivePin->Disconnect();
      FreeMediaType(selected);
      return hr;
    }

    IMemAllocator* allocator = nullptr;
    hr = input->GetAllocator(&allocator);
    if (FAILED(hr)) {
      input->Release();
      receivePin->Disconnect();
      FreeMediaType(selected);
      return hr;
    }

    ALLOCATOR_PROPERTIES requested{};
    requested.cBuffers = 4;
    requested.cbBuffer = kFrameSize;
    requested.cbAlign = 1;
    requested.cbPrefix = 0;
    ALLOCATOR_PROPERTIES actual{};
    hr = allocator->SetProperties(&requested, &actual);
    if (FAILED(hr) || actual.cbBuffer < kFrameSize) {
      allocator->Release();
      input->Release();
      receivePin->Disconnect();
      FreeMediaType(selected);
      return FAILED(hr) ? hr : VFW_E_BUFFER_UNDERFLOW;
    }

    hr = input->NotifyAllocator(allocator, FALSE);
    if (SUCCEEDED(hr)) {
      hr = allocator->Commit();
    }
    if (FAILED(hr)) {
      allocator->Release();
      input->Release();
      receivePin->Disconnect();
      FreeMediaType(selected);
      return hr;
    }

    EnterCriticalSection(&lock_);
    connected_ = receivePin;
    connected_->AddRef();
    input_ = input;
    allocator_ = allocator;
    FreeMediaType(connectionType_);
    connectionType_ = selected;
    hasConnectionType_ = true;
    LeaveCriticalSection(&lock_);

    return S_OK;
  }
  STDMETHODIMP ReceiveConnection(IPin*, const AM_MEDIA_TYPE*) override {
    return VFW_E_INVALID_DIRECTION;
  }
  STDMETHODIMP Disconnect() override {
    StopStreaming();
    EnterCriticalSection(&lock_);
    if (allocator_) {
      allocator_->Decommit();
      allocator_->Release();
      allocator_ = nullptr;
    }
    if (input_) {
      input_->Release();
      input_ = nullptr;
    }
    if (connected_) {
      connected_->Release();
      connected_ = nullptr;
    }
    if (hasConnectionType_) {
      FreeMediaType(connectionType_);
      ZeroMemory(&connectionType_, sizeof(connectionType_));
      hasConnectionType_ = false;
    }
    LeaveCriticalSection(&lock_);
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
  STDMETHODIMP ConnectionMediaType(AM_MEDIA_TYPE* mediaType) override {
    if (!mediaType) {
      return E_POINTER;
    }
    EnterCriticalSection(&lock_);
    const bool connected = hasConnectionType_;
    HRESULT hr = connected ? CopyMediaType(mediaType, &connectionType_)
                           : VFW_E_NOT_CONNECTED;
    LeaveCriticalSection(&lock_);
    return hr;
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
    return IsSupportedMediaType(type) ? S_OK : S_FALSE;
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

  STDMETHODIMP SetFormat(AM_MEDIA_TYPE* type) override {
    Log("IAMStreamConfig::SetFormat");
    if (!type) {
      return E_POINTER;
    }
    if (!IsSupportedMediaType(type)) {
      return VFW_E_TYPE_NOT_ACCEPTED;
    }

    EnterCriticalSection(&lock_);
    if (connected_) {
      LeaveCriticalSection(&lock_);
      return VFW_E_ALREADY_CONNECTED;
    }
    FreeMediaType(preferredType_);
    HRESULT hr = CopyMediaType(&preferredType_, type);
    hasPreferredType_ = SUCCEEDED(hr);
    LeaveCriticalSection(&lock_);
    return hr;
  }

  STDMETHODIMP GetFormat(AM_MEDIA_TYPE** type) override {
    Log("IAMStreamConfig::GetFormat");
    if (!type) {
      return E_POINTER;
    }
    *type = static_cast<AM_MEDIA_TYPE*>(CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE)));
    if (!*type) {
      return E_OUTOFMEMORY;
    }

    EnterCriticalSection(&lock_);
    HRESULT hr = S_OK;
    if (hasConnectionType_) {
      hr = CopyMediaType(*type, &connectionType_);
    } else if (hasPreferredType_) {
      hr = CopyMediaType(*type, &preferredType_);
    } else {
      hr = CreateRgb32MediaType(*type);
    }
    LeaveCriticalSection(&lock_);

    if (FAILED(hr)) {
      CoTaskMemFree(*type);
      *type = nullptr;
    }
    return hr;
  }

  STDMETHODIMP GetNumberOfCapabilities(int* count, int* size) override {
    Log("IAMStreamConfig::GetNumberOfCapabilities");
    if (!count || !size) {
      return E_POINTER;
    }
    *count = 1;
    *size = sizeof(VIDEO_STREAM_CONFIG_CAPS);
    return S_OK;
  }

  STDMETHODIMP GetStreamCaps(int index, AM_MEDIA_TYPE** type, BYTE* caps) override {
    Log("IAMStreamConfig::GetStreamCaps");
    if (index != 0) {
      return S_FALSE;
    }
    if (!type || !caps) {
      return E_POINTER;
    }

    *type = static_cast<AM_MEDIA_TYPE*>(CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE)));
    if (!*type) {
      return E_OUTOFMEMORY;
    }
    HRESULT hr = CreateRgb32MediaType(*type);
    if (FAILED(hr)) {
      CoTaskMemFree(*type);
      *type = nullptr;
      return hr;
    }

    FillVideoStreamCaps(reinterpret_cast<VIDEO_STREAM_CONFIG_CAPS*>(caps));
    return S_OK;
  }

  HRESULT StartStreaming() {
    Log("OutputPin::StartStreaming");
    EnterCriticalSection(&lock_);
    const bool canStart = input_ && allocator_;
    const bool alreadyRunning = thread_ != nullptr;
    if (alreadyRunning) {
      LeaveCriticalSection(&lock_);
      return S_OK;
    }
    if (!canStart) {
      LeaveCriticalSection(&lock_);
      return VFW_E_NOT_CONNECTED;
    }
    ResetEvent(stopEvent_);
    AddRef();
    thread_ = CreateThread(nullptr, 0, &ChinaskiOutputPin::ThreadProc, this, 0,
                           nullptr);
    if (!thread_) {
      SetEvent(stopEvent_);
      Release();
      const HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
      LeaveCriticalSection(&lock_);
      return hr;
    }
    LeaveCriticalSection(&lock_);
    return S_OK;
  }

  void StopStreaming() {
    Log("OutputPin::StopStreaming");
    HANDLE thread = nullptr;
    EnterCriticalSection(&lock_);
    if (thread_) {
      thread = thread_;
      thread_ = nullptr;
      SetEvent(stopEvent_);
    }
    LeaveCriticalSection(&lock_);

    if (thread) {
      WaitForSingleObject(thread, INFINITE);
      CloseHandle(thread);
    }
  }

 private:
  static DWORD WINAPI ThreadProc(void* context) {
    auto* self = static_cast<ChinaskiOutputPin*>(context);
    self->StreamingLoop();
    self->Release();
    return 0;
  }

  void StreamingLoop() {
    REFERENCE_TIME frameStart = 0;
    LONG frameIndex = 0;

    while (WaitForSingleObject(stopEvent_, 0) == WAIT_TIMEOUT) {
      IMediaSample* sample = nullptr;
      IMemAllocator* allocator = nullptr;
      IMemInputPin* input = nullptr;

      EnterCriticalSection(&lock_);
      allocator = allocator_;
      input = input_;
      if (allocator) {
        allocator->AddRef();
      }
      if (input) {
        input->AddRef();
      }
      LeaveCriticalSection(&lock_);

      if (!allocator || !input) {
        if (allocator) {
          allocator->Release();
        }
        if (input) {
          input->Release();
        }
        break;
      }

      HRESULT hr = allocator->GetBuffer(&sample, nullptr, nullptr, 0);
      if (SUCCEEDED(hr) && sample) {
        BYTE* data = nullptr;
        if (SUCCEEDED(sample->GetPointer(&data)) && data) {
          FillFrame(data, frameIndex);
          sample->SetActualDataLength(kFrameSize);
          sample->SetSyncPoint(TRUE);
          sample->SetPreroll(FALSE);
          REFERENCE_TIME frameEnd = frameStart + kFrameDuration;
          sample->SetTime(&frameStart, &frameEnd);
          input->Receive(sample);
          frameStart = frameEnd;
          ++frameIndex;
        }
        sample->Release();
      }

      input->Release();
      allocator->Release();
      WaitForSingleObject(stopEvent_, 33);
    }
  }

  static void FillFrame(BYTE* data, LONG frameIndex) {
    for (LONG y = 0; y < kFrameHeight; ++y) {
      auto* row = reinterpret_cast<DWORD*>(
          data + static_cast<size_t>(y) * kFrameWidth * kFrameBytesPerPixel);
      for (LONG x = 0; x < kFrameWidth; ++x) {
        const BYTE r = static_cast<BYTE>((x + frameIndex * 5) % 256);
        const BYTE g = static_cast<BYTE>((y + frameIndex * 3) % 256);
        const BYTE b = static_cast<BYTE>(((x / 8) ^ (y / 8) ^ frameIndex) % 256);
        row[x] = 0xff000000u | (static_cast<DWORD>(r) << 16) |
                 (static_cast<DWORD>(g) << 8) | static_cast<DWORD>(b);
      }
    }
  }

  long refs_ = 1;
  ChinaskiSourceFilter* filter_ = nullptr;
  IPin* connected_ = nullptr;
  IMemInputPin* input_ = nullptr;
  IMemAllocator* allocator_ = nullptr;
  AM_MEDIA_TYPE connectionType_{};
  bool hasConnectionType_ = false;
  AM_MEDIA_TYPE preferredType_{};
  bool hasPreferredType_ = false;
  CRITICAL_SECTION lock_{};
  HANDLE stopEvent_ = nullptr;
  HANDLE thread_ = nullptr;
};

class ChinaskiSourceFilter final : public IBaseFilter {
 public:
  ChinaskiSourceFilter() {
    pin_ = new (std::nothrow) ChinaskiOutputPin(this);
    InterlockedIncrement(&g_objectCount);
  }

  ~ChinaskiSourceFilter() {
    Stop();
    if (pin_) {
      pin_->Release();
    }
    if (clock_) {
      clock_->Release();
      clock_ = nullptr;
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
    Log("Filter::Stop");
    if (pin_) {
      pin_->StopStreaming();
    }
    state_ = State_Stopped;
    return S_OK;
  }
  STDMETHODIMP Pause() override {
    Log("Filter::Pause");
    if (pin_) {
      HRESULT hr = pin_->StartStreaming();
      if (FAILED(hr) && hr != VFW_E_NOT_CONNECTED) {
        return hr;
      }
    }
    state_ = State_Paused;
    return S_OK;
  }
  STDMETHODIMP Run(REFERENCE_TIME start) override {
    Log("Filter::Run");
    start_ = start;
    if (pin_) {
      HRESULT hr = pin_->StartStreaming();
      if (FAILED(hr)) {
        return hr;
      }
    }
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
  if (riid == IID_IAMStreamConfig) {
    *object = static_cast<IAMStreamConfig*>(this);
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
    Log("ClassFactory::CreateInstance");
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
  Log("CreateChinaskiDirectShowFilter");
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
  Log("DllGetClassObject");
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
