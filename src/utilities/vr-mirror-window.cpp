// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <VDXROVR/VRMirrorSHM.hpp>
#include <VDXROVR/WASAPIProcessLoopback.hpp>

#include <Windows.h>
#include <d3d11.h>
#include <directxtk/CommonStates.h>
#include <directxtk/SpriteBatch.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>

using Microsoft::WRL::ComPtr;
using namespace VDXROVR;

namespace {

constexpr wchar_t kWindowClassName[] = L"VDXROVRVRMirrorWindowClass";
constexpr wchar_t kWindowTitle[] = L"VDXROVR VDXR Mirror";

class MirrorWindowApp final {
 public:
  ~MirrorWindowApp() { this->Shutdown(); }

  int Run(HINSTANCE hInstance, const int nCmdShow) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    WNDCLASSEXW wc {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = &MirrorWindowApp::StaticWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClassName;
    if (!RegisterClassExW(&wc)) {
      return 1;
    }

    const int initW = 1280;
    const int initH = 720;
    const int screenW = GetSystemMetrics(SM_CXSCREEN);
    const int screenH = GetSystemMetrics(SM_CYSCREEN);
    const int posX = (std::max)(0, (screenW - initW) / 2);
    const int posY = (std::max)(0, (screenH - initH) / 2);

    // Pure borderless 16:9 popup window (zero titlebar, zero borders, zero chrome)
    mHwnd = CreateWindowExW(
      0,
      kWindowClassName,
      kWindowTitle,
      WS_POPUP | WS_VISIBLE,
      posX,
      posY,
      initW,
      initH,
      nullptr,
      nullptr,
      hInstance,
      this);
    if (!mHwnd) {
      return 1;
    }

    if (!this->InitD3D(initW, initH)) {
      return 1;
    }

    ShowWindow(mHwnd, nCmdShow);
    UpdateWindow(mHwnd);

    MSG msg {};
    while (mRunning) {
      while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
          mRunning = false;
          break;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
      }
      if (!mRunning) {
        break;
      }
      this->TickAndRender();
    }

    this->Shutdown();
    return 0;
  }

 private:
  static LRESULT CALLBACK StaticWndProc(
    HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam) {
    MirrorWindowApp* self = nullptr;
    if (msg == WM_NCCREATE) {
      const auto* cs = reinterpret_cast<const CREATESTRUCTW*>(lParam);
      self = static_cast<MirrorWindowApp*>(cs->lpCreateParams);
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
      self = reinterpret_cast<MirrorWindowApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self) {
      return self->WndProc(hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
  }

  LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
      case WM_CLOSE:
      case WM_DESTROY:
        mRunning = false;
        PostQuitMessage(0);
        return 0;
      case WM_NCHITTEST: {
        // Allow dragging borderless window from anywhere on the 16:9 frame,
        // while keeping bottom-right corner resizable when windowed.
        if (mIsFullscreen) {
          return HTCLIENT;
        }
        POINT pt {
          static_cast<int>(static_cast<short>(LOWORD(lParam))),
          static_cast<int>(static_cast<short>(HIWORD(lParam)))};
        ScreenToClient(hwnd, &pt);
        RECT rc {};
        GetClientRect(hwnd, &rc);
        constexpr int kGrip = 14;
        if (pt.x >= rc.right - kGrip && pt.y >= rc.bottom - kGrip) {
          return HTBOTTOMRIGHT;
        }
        return HTCAPTION;
      }
      case WM_SIZING: {
        // Strictly lock aspect ratio to 16:9 during resize
        auto* r = reinterpret_cast<RECT*>(lParam);
        const int w = (std::max)(320, static_cast<int>(r->right - r->left));
        const int h = (std::max)(180, static_cast<int>(std::lround(w * (9.0 / 16.0))));
        r->right = r->left + w;
        r->bottom = r->top + h;
        return TRUE;
      }
      case WM_SIZE: {
        const uint32_t newW = LOWORD(lParam);
        const uint32_t newH = HIWORD(lParam);
        if (newW > 0u && newH > 0u && mSwapChain) {
          this->ResizeSwapChain(newW, newH);
        }
        return 0;
      }
      case WM_NCLBUTTONDBLCLK:
      case WM_LBUTTONDBLCLK:
        this->ToggleFullscreen();
        return 0;
      case WM_KEYDOWN: {
        const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        if (wParam == VK_ESCAPE) {
          PostMessageW(hwnd, WM_CLOSE, 0, 0);
          return 0;
        }
        if (wParam == VK_F11) {
          this->ToggleFullscreen();
          return 0;
        }
        if (ctrl && wParam == 'M') {
          // Local audio monitor toggle (default OFF to prevent headset echo)
          mLocalAudioMonitorOverride = true;
          mAudioMonitorEnabled = !mAudioMonitorEnabled;
          this->SyncAudioCapture(mActiveGamePid);
          return 0;
        }
        if (ctrl && mSharedBuffer) {
          if (wParam == '1') {
            VRMirrorWriteU32Atomic(&mSharedBuffer->mHeader.mConsumerDesiredResolution, 0u);
          } else if (wParam == '2') {
            VRMirrorWriteU32Atomic(&mSharedBuffer->mHeader.mConsumerDesiredResolution, 1u);
          } else if (wParam == 'L') {
            VRMirrorWriteU32Atomic(&mSharedBuffer->mHeader.mConsumerDesiredEyeMode, 0u);
          } else if (wParam == 'R') {
            VRMirrorWriteU32Atomic(&mSharedBuffer->mHeader.mConsumerDesiredEyeMode, 1u);
          } else if (wParam == 'B') {
            VRMirrorWriteU32Atomic(&mSharedBuffer->mHeader.mConsumerDesiredEyeMode, 2u);
          } else if (wParam == 'O') {
            const uint32_t cur = VRMirrorReadU32Atomic(
              &mSharedBuffer->mHeader.mConsumerDesiredShowOverlays);
            const uint32_t curEffective = (cur <= 1u)
              ? cur
              : static_cast<uint32_t>(mSharedBuffer->mHeader.mShowOverlays != 0u ? 1u : 0u);
            VRMirrorWriteU32Atomic(
              &mSharedBuffer->mHeader.mConsumerDesiredShowOverlays,
              curEffective ^ 1u);
          }
        }
        return 0;
      }
      default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
  }

  void ToggleFullscreen() {
    if (!mIsFullscreen) {
      GetWindowRect(mHwnd, &mWindowedRect);
      HMONITOR mon = MonitorFromWindow(mHwnd, MONITOR_DEFAULTTONEAREST);
      MONITORINFO mi {.cbSize = sizeof(mi)};
      if (GetMonitorInfoW(mon, &mi)) {
        mIsFullscreen = true;
        SetWindowPos(
          mHwnd,
          HWND_TOP,
          mi.rcMonitor.left,
          mi.rcMonitor.top,
          mi.rcMonitor.right - mi.rcMonitor.left,
          mi.rcMonitor.bottom - mi.rcMonitor.top,
          SWP_FRAMECHANGED | SWP_SHOWWINDOW);
      }
    } else {
      mIsFullscreen = false;
      SetWindowPos(
        mHwnd,
        HWND_NOTOPMOST,
        mWindowedRect.left,
        mWindowedRect.top,
        mWindowedRect.right - mWindowedRect.left,
        mWindowedRect.bottom - mWindowedRect.top,
        SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }
  }

  bool InitD3D(const uint32_t width, const uint32_t height) {
    D3D_FEATURE_LEVEL levels[] {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    HRESULT hr = D3D11CreateDevice(
      nullptr,
      D3D_DRIVER_TYPE_HARDWARE,
      nullptr,
      D3D11_CREATE_DEVICE_BGRA_SUPPORT,
      levels,
      ARRAYSIZE(levels),
      D3D11_SDK_VERSION,
      mDevice.GetAddressOf(),
      nullptr,
      mContext.GetAddressOf());
    if (FAILED(hr)) {
      return false;
    }

    ComPtr<IDXGIDevice> dxgiDevice;
    ComPtr<IDXGIAdapter> adapter;
    ComPtr<IDXGIFactory2> factory;
    if (
      FAILED(mDevice.As(&dxgiDevice))
      || FAILED(dxgiDevice->GetAdapter(adapter.GetAddressOf()))
      || FAILED(adapter->GetParent(IID_PPV_ARGS(factory.GetAddressOf())))) {
      return false;
    }

    DXGI_SWAP_CHAIN_DESC1 scDesc {};
    scDesc.Width = width;
    scDesc.Height = height;
    scDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scDesc.SampleDesc = {1u, 0u};
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.BufferCount = 2u;
    scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    hr = factory->CreateSwapChainForHwnd(
      mDevice.Get(), mHwnd, &scDesc, nullptr, nullptr, mSwapChain.GetAddressOf());
    if (FAILED(hr)) {
      return false;
    }

    mSpriteBatch = std::make_unique<DirectX::SpriteBatch>(mContext.Get());
    mCommonStates = std::make_unique<DirectX::CommonStates>(mDevice.Get());
    return this->RecreateRenderTarget(width, height);
  }

  bool RecreateRenderTarget(const uint32_t width, const uint32_t height) {
    mRtv.Reset();
    mBackBuffer.Reset();
    ComPtr<ID3D11Texture2D> bb;
    if (FAILED(mSwapChain->GetBuffer(0u, IID_PPV_ARGS(bb.GetAddressOf())))) {
      return false;
    }
    if (FAILED(mDevice->CreateRenderTargetView(bb.Get(), nullptr, mRtv.GetAddressOf()))) {
      return false;
    }
    mBackBuffer = bb;
    mClientWidth = width;
    mClientHeight = height;
    return true;
  }

  void ResizeSwapChain(const uint32_t width, const uint32_t height) {
    if (!mSwapChain || width == 0u || height == 0u) {
      return;
    }
    mRtv.Reset();
    mBackBuffer.Reset();
    if (SUCCEEDED(mSwapChain->ResizeBuffers(
          0u, width, height, DXGI_FORMAT_UNKNOWN, 0u))) {
      this->RecreateRenderTarget(width, height);
    }
  }

  bool EnsureUploadTexture(const uint32_t width, const uint32_t height) {
    if (
      mUploadTex && mUploadSrv && mUploadWidth == width
      && mUploadHeight == height) {
      return true;
    }
    mUploadSrv.Reset();
    mUploadTex.Reset();
    D3D11_TEXTURE2D_DESC desc {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1u;
    desc.ArraySize = 1u;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc = {1u, 0u};
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(mDevice->CreateTexture2D(&desc, nullptr, mUploadTex.GetAddressOf()))) {
      return false;
    }
    if (FAILED(mDevice->CreateShaderResourceView(
          mUploadTex.Get(), nullptr, mUploadSrv.GetAddressOf()))) {
      mUploadTex.Reset();
      return false;
    }
    mUploadWidth = width;
    mUploadHeight = height;
    return true;
  }

  void EnsureSharedMemory() {
    if (mSharedBuffer) {
      return;
    }
    const uint64_t totalBytes = sizeof(VRMirrorSharedBuffer);
    mMappingHandle = CreateFileMappingW(
      INVALID_HANDLE_VALUE,
      nullptr,
      PAGE_READWRITE,
      static_cast<DWORD>(totalBytes >> 32),
      static_cast<DWORD>(totalBytes & 0xFFFFFFFFull),
      kVRMirrorSharedMemoryName);
    if (!mMappingHandle) {
      return;
    }
    const bool newlyCreated = (GetLastError() != ERROR_ALREADY_EXISTS);
    void* mapped = MapViewOfFile(
      mMappingHandle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(VRMirrorSharedBuffer));
    if (!mapped) {
      CloseHandle(mMappingHandle);
      mMappingHandle = nullptr;
      return;
    }
    mSharedBuffer = static_cast<VRMirrorSharedBuffer*>(mapped);
    if (
      newlyCreated
      || mSharedBuffer->mHeader.mMagic != kVRMirrorSharedMagic
      || mSharedBuffer->mHeader.mVersion != kVRMirrorSharedVersion) {
      std::memset(&mSharedBuffer->mHeader, 0, sizeof(VRMirrorSharedHeader));
      mSharedBuffer->mHeader.mMagic = kVRMirrorSharedMagic;
      mSharedBuffer->mHeader.mVersion = kVRMirrorSharedVersion;
      mSharedBuffer->mHeader.mPixelFormat = kVRMirrorPixelFormatBGRA8;
      mSharedBuffer->mHeader.mConsumerDesiredEyeMode = kVRMirrorUseAppSetting;
      mSharedBuffer->mHeader.mConsumerDesiredResolution = kVRMirrorUseAppSetting;
      mSharedBuffer->mHeader.mConsumerDesiredShowOverlays = kVRMirrorUseAppSetting;
    }
  }

  void SyncAudioCapture(const uint32_t gamePid) {
    const bool shouldCapture = mAudioMonitorEnabled && (gamePid != 0u);
    if (!shouldCapture) {
      mLoopback.Stop();
      mMonitorOutput.Close();
      mActiveAudioPid = 0u;
      return;
    }
    if (mActiveAudioPid == gamePid && mLoopback.IsActive()) {
      return;
    }
    mLoopback.Stop();
    if (mMonitorOutput.Open(WASAPIProcessLoopbackCapture::kSampleRate)) {
      if (mLoopback.Start(
            gamePid,
            [this](
              const float* samples,
              const uint32_t frames,
              const uint32_t,
              const uint64_t) { mMonitorOutput.WriteFrames(samples, frames); })) {
        mActiveAudioPid = gamePid;
      }
    }
  }

  void TickAndRender() {
    this->EnsureSharedMemory();
    const uint64_t nowMs = GetTickCount64();

    bool hasLiveFrame = false;
    if (mSharedBuffer) {
      VRMirrorWriteU64Atomic(
        &mSharedBuffer->mHeader.mWindowConsumerHeartbeatMs, nowMs);

      const bool prodActive = IsVRMirrorProducerActive(mSharedBuffer->mHeader, nowMs);
      const uint32_t gamePid =
        prodActive ? VRMirrorReadU32Atomic(&mSharedBuffer->mHeader.mGameProcessId) : 0u;
      mActiveGamePid = gamePid;

      if (!mLocalAudioMonitorOverride && prodActive) {
        mAudioMonitorEnabled =
          (mSharedBuffer->mHeader.mCaptureGameAudio != 0u)
          && (mSharedBuffer->mHeader.mWindowAudioMonitor != 0u);
      }
      this->SyncAudioCapture(gamePid);

      if (prodActive && mSharedBuffer->mHeader.mHasPixelData != 0u) {
        const uint64_t seq =
          VRMirrorReadU64Atomic(&mSharedBuffer->mHeader.mFrameSequence);
        const uint32_t slot =
          VRMirrorReadU32Atomic(&mSharedBuffer->mHeader.mActiveSlot) & 1u;
        const uint32_t fw = mSharedBuffer->mHeader.mWidth;
        const uint32_t fh = mSharedBuffer->mHeader.mHeight;
        if (
          fw > 0u && fh > 0u && fw <= kVRMirrorMaxWidth
          && fh <= kVRMirrorMaxHeight && this->EnsureUploadTexture(fw, fh)) {
          if (seq != mLastFrameSeq) {
            std::atomic_thread_fence(std::memory_order_acquire);
            mContext->UpdateSubresource(
              mUploadTex.Get(),
              0u,
              nullptr,
              mSharedBuffer->mSlots[slot],
              fw * 4u,
              0u);
            mLastFrameSeq = seq;
          }
          hasLiveFrame = true;
        }
      }
    }

    if (mRtv && mBackBuffer) {
      const float idleDark[4] {0.039f, 0.051f, 0.078f, 1.0f};
      ID3D11RenderTargetView* rtv = mRtv.Get();
      mContext->OMSetRenderTargets(1u, &rtv, nullptr);
      mContext->ClearRenderTargetView(rtv, idleDark);

      if (hasLiveFrame && mUploadTex) {
        if (mUploadWidth == mClientWidth && mUploadHeight == mClientHeight) {
          mContext->CopyResource(mBackBuffer.Get(), mUploadTex.Get());
        } else if (mSpriteBatch && mCommonStates && mUploadSrv) {
          const D3D11_VIEWPORT vp {
            .TopLeftX = 0.0f,
            .TopLeftY = 0.0f,
            .Width = static_cast<float>(mClientWidth),
            .Height = static_cast<float>(mClientHeight),
            .MinDepth = 0.0f,
            .MaxDepth = 1.0f,
          };
          mContext->RSSetViewports(1u, &vp);
          mSpriteBatch->SetViewport(vp);
          const RECT dstRect {
            0,
            0,
            static_cast<LONG>(mClientWidth),
            static_cast<LONG>(mClientHeight),
          };
          mSpriteBatch->Begin(
            DirectX::SpriteSortMode_Immediate,
            mCommonStates->Opaque(),
            mCommonStates->LinearClamp());
          mSpriteBatch->Draw(mUploadSrv.Get(), dstRect);
          mSpriteBatch->End();
        }
      }

      mSwapChain->Present(1u, 0u);
    } else {
      Sleep(16u);
    }
  }

  void Shutdown() {
    mLoopback.Stop();
    mMonitorOutput.Close();
    if (mSharedBuffer) {
      VRMirrorWriteU64Atomic(&mSharedBuffer->mHeader.mWindowConsumerHeartbeatMs, 0u);
      VRMirrorWriteU32Atomic(
        &mSharedBuffer->mHeader.mConsumerDesiredEyeMode, kVRMirrorUseAppSetting);
      VRMirrorWriteU32Atomic(
        &mSharedBuffer->mHeader.mConsumerDesiredResolution, kVRMirrorUseAppSetting);
      VRMirrorWriteU32Atomic(
        &mSharedBuffer->mHeader.mConsumerDesiredShowOverlays, kVRMirrorUseAppSetting);
      UnmapViewOfFile(mSharedBuffer);
      mSharedBuffer = nullptr;
    }
    if (mMappingHandle) {
      CloseHandle(mMappingHandle);
      mMappingHandle = nullptr;
    }
    mSpriteBatch.reset();
    mCommonStates.reset();
    mUploadSrv.Reset();
    mUploadTex.Reset();
    mRtv.Reset();
    mBackBuffer.Reset();
    mSwapChain.Reset();
    mContext.Reset();
    mDevice.Reset();
  }

  HWND mHwnd {nullptr};
  bool mRunning {true};
  bool mIsFullscreen {false};
  RECT mWindowedRect {100, 100, 1380, 820};
  uint32_t mClientWidth {1280};
  uint32_t mClientHeight {720};

  ComPtr<ID3D11Device> mDevice;
  ComPtr<ID3D11DeviceContext> mContext;
  ComPtr<IDXGISwapChain1> mSwapChain;
  ComPtr<ID3D11Texture2D> mBackBuffer;
  ComPtr<ID3D11RenderTargetView> mRtv;
  ComPtr<ID3D11Texture2D> mUploadTex;
  ComPtr<ID3D11ShaderResourceView> mUploadSrv;
  std::unique_ptr<DirectX::SpriteBatch> mSpriteBatch;
  std::unique_ptr<DirectX::CommonStates> mCommonStates;
  uint32_t mUploadWidth {0};
  uint32_t mUploadHeight {0};

  HANDLE mMappingHandle {nullptr};
  VRMirrorSharedBuffer* mSharedBuffer {nullptr};
  uint64_t mLastFrameSeq {0};

  bool mLocalAudioMonitorOverride {false};
  bool mAudioMonitorEnabled {false};
  uint32_t mActiveGamePid {0};
  uint32_t mActiveAudioPid {0};
  WASAPIProcessLoopbackCapture mLoopback;
  WASAPIStereoMonitorOutput mMonitorOutput;
};

}// namespace

int WINAPI wWinMain(
  HINSTANCE hInstance,
  HINSTANCE,
  PWSTR,
  int nCmdShow) {
  MirrorWindowApp app;
  return app.Run(hInstance, nCmdShow);
}
