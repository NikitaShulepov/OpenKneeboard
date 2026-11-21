// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <Windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <objbase.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace VDXROVR {

// Captures audio exclusively from a target VR game process tree via Windows
// WASAPI Process Loopback (Windows 10 20H1+ / Windows 11), excluding system
// sounds, voice chat (Discord), browser tabs, and OBS itself.
class WASAPIProcessLoopbackCapture final {
 public:
  using AudioFrameCallback = std::function<void(
    const float* interleavedStereo,
    uint32_t frameCount,
    uint32_t sampleRate,
    uint64_t timestampNs)>;

  static constexpr uint32_t kSampleRate = 48000u;
  static constexpr uint16_t kChannels = 2u;

  WASAPIProcessLoopbackCapture() = default;
  ~WASAPIProcessLoopbackCapture() { this->Stop(); }

  WASAPIProcessLoopbackCapture(const WASAPIProcessLoopbackCapture&) = delete;
  WASAPIProcessLoopbackCapture& operator=(
    const WASAPIProcessLoopbackCapture&) = delete;

  bool Start(const uint32_t targetPid, AudioFrameCallback callback) {
    this->Stop();
    if (targetPid == 0u || !callback) {
      return false;
    }

    mTargetPid.store(targetPid, std::memory_order_release);
    mStopRequested.store(false, std::memory_order_release);
    mStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!mStopEvent) {
      return false;
    }

    {
      std::scoped_lock lock(mCallbackMutex);
      mCallback = std::move(callback);
    }

    mThread = std::thread([this, targetPid]() { this->CaptureLoop(targetPid); });
    return true;
  }

  void Stop() noexcept {
    mStopRequested.store(true, std::memory_order_release);
    if (mStopEvent) {
      SetEvent(mStopEvent);
    }
    if (mThread.joinable()) {
      mThread.join();
    }
    if (mStopEvent) {
      CloseHandle(mStopEvent);
      mStopEvent = nullptr;
    }
    {
      std::scoped_lock lock(mCallbackMutex);
      mCallback = nullptr;
    }
    mTargetPid.store(0u, std::memory_order_release);
    mActive.store(false, std::memory_order_release);
  }

  [[nodiscard]] uint32_t GetTargetProcessId() const noexcept {
    return mTargetPid.load(std::memory_order_acquire);
  }

  [[nodiscard]] bool IsActive() const noexcept {
    return mActive.load(std::memory_order_acquire);
  }

 private:
  class ActivationHandler final
    : public IActivateAudioInterfaceCompletionHandler,
      public IAgileObject {
   public:
    ActivationHandler()
      : mCompletedEvent(CreateEventW(nullptr, TRUE, FALSE, nullptr)) {}

    ~ActivationHandler() {
      if (mAudioClient) {
        mAudioClient->Release();
        mAudioClient = nullptr;
      }
      if (mCompletedEvent) {
        CloseHandle(mCompletedEvent);
        mCompletedEvent = nullptr;
      }
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
      return static_cast<ULONG>(InterlockedIncrement(&mRefCount));
    }

    ULONG STDMETHODCALLTYPE Release() override {
      const ULONG count = static_cast<ULONG>(InterlockedDecrement(&mRefCount));
      if (count == 0u) {
        delete this;
      }
      return count;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(
      REFIID riid,
      void** ppvObject) override {
      if (!ppvObject) {
        return E_POINTER;
      }
      if (
        riid == __uuidof(IUnknown)
        || riid == __uuidof(IActivateAudioInterfaceCompletionHandler)) {
        *ppvObject =
          static_cast<IActivateAudioInterfaceCompletionHandler*>(this);
        this->AddRef();
        return S_OK;
      }
      if (riid == __uuidof(IAgileObject)) {
        *ppvObject = static_cast<IAgileObject*>(this);
        this->AddRef();
        return S_OK;
      }
      *ppvObject = nullptr;
      return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE ActivateCompleted(
      IActivateAudioInterfaceAsyncOperation* operation) override {
      HRESULT activateHr = E_FAIL;
      IUnknown* unk = nullptr;
      const HRESULT hr = operation->GetActivateResult(&activateHr, &unk);
      if (SUCCEEDED(hr) && SUCCEEDED(activateHr) && unk) {
        mActivateResult =
          unk->QueryInterface(__uuidof(IAudioClient), reinterpret_cast<void**>(&mAudioClient));
        unk->Release();
      } else {
        mActivateResult = FAILED(hr) ? hr : activateHr;
      }
      if (mCompletedEvent) {
        SetEvent(mCompletedEvent);
      }
      return S_OK;
    }

    bool Wait(const DWORD timeoutMs) const noexcept {
      return mCompletedEvent
        && WaitForSingleObject(mCompletedEvent, timeoutMs) == WAIT_OBJECT_0;
    }

    HRESULT GetResult() const noexcept { return mActivateResult; }

    IAudioClient* DetachClient() noexcept {
      IAudioClient* out = mAudioClient;
      mAudioClient = nullptr;
      return out;
    }

   private:
    volatile LONG mRefCount {1};
    HANDLE mCompletedEvent {nullptr};
    HRESULT mActivateResult {E_FAIL};
    IAudioClient* mAudioClient {nullptr};
  };

  static uint64_t QueryTimeNs() noexcept {
    static LARGE_INTEGER sFreq = []() {
      LARGE_INTEGER f {};
      QueryPerformanceFrequency(&f);
      return f;
    }();
    LARGE_INTEGER now {};
    QueryPerformanceCounter(&now);
    if (sFreq.QuadPart <= 0) {
      return 0u;
    }
    const uint64_t q = static_cast<uint64_t>(now.QuadPart);
    const uint64_t f = static_cast<uint64_t>(sFreq.QuadPart);
    return (q / f) * 1000000000ull + ((q % f) * 1000000000ull) / f;
  }

  void CaptureLoop(const uint32_t targetPid) noexcept {
    const HRESULT comHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool comInit = SUCCEEDED(comHr);

    auto cleanupCom = [&]() {
      if (comInit) {
        CoUninitialize();
      }
    };

    AUDIOCLIENT_ACTIVATION_PARAMS activationParams {};
    activationParams.ActivationType =
      AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
    activationParams.ProcessLoopbackParams.TargetProcessId = targetPid;
    activationParams.ProcessLoopbackParams.ProcessLoopbackMode =
      PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;

    PROPVARIANT prop {};
    prop.vt = VT_BLOB;
    prop.blob.cbSize = sizeof(activationParams);
    prop.blob.pBlobData = reinterpret_cast<BYTE*>(&activationParams);

    auto* handler = new ActivationHandler();
    IActivateAudioInterfaceAsyncOperation* asyncOp = nullptr;
    const HRESULT actHr = ActivateAudioInterfaceAsync(
      VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK,
      __uuidof(IAudioClient),
      &prop,
      handler,
      &asyncOp);

    if (FAILED(actHr) || !asyncOp) {
      handler->Release();
      cleanupCom();
      return;
    }

    const bool waited = handler->Wait(2500u);
    asyncOp->Release();

    IAudioClient* audioClient = nullptr;
    if (waited && SUCCEEDED(handler->GetResult())) {
      audioClient = handler->DetachClient();
    }
    handler->Release();

    if (!audioClient) {
      cleanupCom();
      return;
    }

    WAVEFORMATEX wfx {};
    wfx.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    wfx.nChannels = kChannels;
    wfx.nSamplesPerSec = kSampleRate;
    wfx.wBitsPerSample = 32u;
    wfx.nBlockAlign =
      static_cast<WORD>(wfx.nChannels * (wfx.wBitsPerSample / 8u));
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    wfx.cbSize = 0u;

    const DWORD streamFlags = AUDCLNT_STREAMFLAGS_LOOPBACK
      | AUDCLNT_STREAMFLAGS_EVENTCALLBACK
      | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
      | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;

    HRESULT hr = audioClient->Initialize(
      AUDCLNT_SHAREMODE_SHARED,
      streamFlags,
      200000, // 20 ms buffer
      0,
      &wfx,
      nullptr);

    HANDLE sampleEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    IAudioCaptureClient* captureClient = nullptr;

    if (SUCCEEDED(hr) && sampleEvent) {
      hr = audioClient->SetEventHandle(sampleEvent);
    }
    if (SUCCEEDED(hr)) {
      hr = audioClient->GetService(
        __uuidof(IAudioCaptureClient),
        reinterpret_cast<void**>(&captureClient));
    }
    if (SUCCEEDED(hr) && captureClient) {
      hr = audioClient->Start();
    }

    if (SUCCEEDED(hr) && captureClient && sampleEvent) {
      mActive.store(true, std::memory_order_release);
      std::vector<float> silenceBuf;
      HANDLE waitHandles[2] {mStopEvent, sampleEvent};

      while (!mStopRequested.load(std::memory_order_acquire)) {
        const DWORD waitRes =
          WaitForMultipleObjects(2u, waitHandles, FALSE, 50u);
        if (waitRes == WAIT_OBJECT_0) {
          break;
        }

        UINT32 packetFrames = 0u;
        while (SUCCEEDED(captureClient->GetNextPacketSize(&packetFrames))
               && packetFrames > 0u) {
          BYTE* data = nullptr;
          UINT32 numFrames = 0u;
          DWORD flags = 0u;
          const HRESULT getHr = captureClient->GetBuffer(
            &data, &numFrames, &flags, nullptr, nullptr);
          if (FAILED(getHr)) {
            break;
          }
          if (numFrames > 0u) {
            const float* samples = nullptr;
            if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0u || !data) {
              const size_t needed =
                static_cast<size_t>(numFrames) * kChannels;
              if (silenceBuf.size() < needed) {
                silenceBuf.assign(needed, 0.0f);
              }
              samples = silenceBuf.data();
            } else {
              samples = reinterpret_cast<const float*>(data);
            }

            AudioFrameCallback cb;
            {
              std::scoped_lock lock(mCallbackMutex);
              cb = mCallback;
            }
            if (cb) {
              cb(samples, numFrames, kSampleRate, QueryTimeNs());
            }
          }
          captureClient->ReleaseBuffer(numFrames);
        }
      }
      audioClient->Stop();
    }

    mActive.store(false, std::memory_order_release);
    if (captureClient) {
      captureClient->Release();
    }
    if (sampleEvent) {
      CloseHandle(sampleEvent);
    }
    audioClient->Release();
    cleanupCom();
  }

  std::atomic<uint32_t> mTargetPid {0u};
  std::atomic<bool> mStopRequested {false};
  std::atomic<bool> mActive {false};
  HANDLE mStopEvent {nullptr};
  std::mutex mCallbackMutex;
  AudioFrameCallback mCallback;
  std::thread mThread;
};

// Optional local playback renderer for the standalone borderless 16:9 Mirror
// window (disabled by default to avoid acoustic echo with the VR headset).
class WASAPIStereoMonitorOutput final {
 public:
  WASAPIStereoMonitorOutput() = default;
  ~WASAPIStereoMonitorOutput() { this->Close(); }

  WASAPIStereoMonitorOutput(const WASAPIStereoMonitorOutput&) = delete;
  WASAPIStereoMonitorOutput& operator=(const WASAPIStereoMonitorOutput&) = delete;

  bool Open(const uint32_t sampleRate = 48000u) noexcept {
    std::scoped_lock lock(mMutex);
    if (mRenderClient) {
      return true;
    }

    const HRESULT comHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    mComInitialized = SUCCEEDED(comHr);

    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT hr = CoCreateInstance(
      __uuidof(MMDeviceEnumerator),
      nullptr,
      CLSCTX_ALL,
      __uuidof(IMMDeviceEnumerator),
      reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr) || !enumerator) {
      this->CloseLocked();
      return false;
    }

    IMMDevice* device = nullptr;
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    enumerator->Release();
    if (FAILED(hr) || !device) {
      this->CloseLocked();
      return false;
    }

    hr = device->Activate(
      __uuidof(IAudioClient),
      CLSCTX_ALL,
      nullptr,
      reinterpret_cast<void**>(&mAudioClient));
    device->Release();
    if (FAILED(hr) || !mAudioClient) {
      this->CloseLocked();
      return false;
    }

    WAVEFORMATEX wfx {};
    wfx.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    wfx.nChannels = 2u;
    wfx.nSamplesPerSec = sampleRate;
    wfx.wBitsPerSample = 32u;
    wfx.nBlockAlign = 8u;
    wfx.nAvgBytesPerSec = sampleRate * 8u;
    wfx.cbSize = 0u;

    hr = mAudioClient->Initialize(
      AUDCLNT_SHAREMODE_SHARED,
      AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
      500000, // 50 ms
      0,
      &wfx,
      nullptr);
    if (FAILED(hr)) {
      this->CloseLocked();
      return false;
    }

    hr = mAudioClient->GetBufferSize(&mBufferFrameCount);
    if (FAILED(hr) || mBufferFrameCount == 0u) {
      this->CloseLocked();
      return false;
    }

    hr = mAudioClient->GetService(
      __uuidof(IAudioRenderClient),
      reinterpret_cast<void**>(&mRenderClient));
    if (FAILED(hr) || !mRenderClient) {
      this->CloseLocked();
      return false;
    }

    if (FAILED(mAudioClient->Start())) {
      this->CloseLocked();
      return false;
    }
    return true;
  }

  void WriteFrames(
    const float* interleavedStereo,
    const uint32_t frameCount) noexcept {
    std::scoped_lock lock(mMutex);
    if (!mAudioClient || !mRenderClient || !interleavedStereo || frameCount == 0u) {
      return;
    }
    UINT32 padding = 0u;
    if (FAILED(mAudioClient->GetCurrentPadding(&padding))) {
      return;
    }
    if (padding >= mBufferFrameCount) {
      return;
    }
    const UINT32 available = mBufferFrameCount - padding;
    const UINT32 toWrite = (std::min)(available, frameCount);
    if (toWrite == 0u) {
      return;
    }
    BYTE* dest = nullptr;
    if (SUCCEEDED(mRenderClient->GetBuffer(toWrite, &dest)) && dest) {
      std::memcpy(
        dest,
        interleavedStereo,
        static_cast<size_t>(toWrite) * 2u * sizeof(float));
      mRenderClient->ReleaseBuffer(toWrite, 0u);
    }
  }

  void Close() noexcept {
    std::scoped_lock lock(mMutex);
    this->CloseLocked();
  }

 private:
  void CloseLocked() noexcept {
    if (mAudioClient) {
      mAudioClient->Stop();
    }
    if (mRenderClient) {
      mRenderClient->Release();
      mRenderClient = nullptr;
    }
    if (mAudioClient) {
      mAudioClient->Release();
      mAudioClient = nullptr;
    }
    mBufferFrameCount = 0u;
    if (mComInitialized) {
      CoUninitialize();
      mComInitialized = false;
    }
  }

  std::mutex mMutex;
  IAudioClient* mAudioClient {nullptr};
  IAudioRenderClient* mRenderClient {nullptr};
  UINT32 mBufferFrameCount {0u};
  bool mComInitialized {false};
};

}// namespace VDXROVR
