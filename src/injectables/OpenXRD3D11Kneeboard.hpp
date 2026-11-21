// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include "OpenXRKneeboard.hpp"

#include <VDXROVR/D3D11.hpp>
#include <VDXROVR/D3D11/Renderer.hpp>
#include <VDXROVR/SHM/D3D11.hpp>

#include <VDXROVR/config.hpp>

#include <directxtk/CommonStates.h>
#include <directxtk/SpriteBatch.h>

#include <array>
#include <deque>
#include <mutex>
#include <vector>

struct XrGraphicsBindingD3D11KHR;

namespace VDXROVR {

class OpenXRD3D11Kneeboard final : public OpenXRKneeboard {
 public:
  OpenXRD3D11Kneeboard(
    XrInstance,
    XrSystemId,
    XrSession,
    OpenXRRuntimeID,
    const std::shared_ptr<OpenXRNext>&,
    const XrGraphicsBindingD3D11KHR&);
  ~OpenXRD3D11Kneeboard();

  struct DXGIFormats {
    DXGI_FORMAT mTextureFormat;
    DXGI_FORMAT mRenderTargetViewFormat;
  };
  static DXGIFormats GetDXGIFormats(OpenXRNext*, XrSession);

  void OnAppSwapchainCreated(
    XrSwapchain swapchain,
    const XrSwapchainCreateInfo& createInfo) override;
  void OnAppSwapchainDestroyed(XrSwapchain swapchain) override;
  void OnAppSwapchainImageAcquired(
    XrSwapchain swapchain,
    uint32_t imageIndex) override;
  void OnAppSwapchainImageReleased(XrSwapchain swapchain) override;

 protected:
  VRMirrorGraphicsApi GetMirrorGraphicsApi() const noexcept override {
    return VRMirrorGraphicsApi::D3D11;
  }
  bool CaptureMirrorFrame(
    const OpenXRMirror::CaptureRequest& request,
    std::vector<uint8_t>& outBgra,
    uint32_t& outWidth,
    uint32_t& outHeight,
    uint32_t& outSourceWidth,
    uint32_t& outSourceHeight) override;
  SHM::Reader& GetSHM() override;
  XrSwapchain CreateSwapchain(XrSession, const PixelSize&) override;
  void ReleaseSwapchainResources(XrSwapchain) override;

  void RenderLayers(
    XrSwapchain swapchain,
    uint32_t swapchainTextureIndex,
    SHM::Frame,
    const std::span<SHM::LayerSprite>& layers) override;

 private:
  std::unique_ptr<SHM::D3D11::Reader> mSHM;

  winrt::com_ptr<ID3D11Device> mDevice;
  winrt::com_ptr<ID3D11DeviceContext1> mImmediateContext;
  std::unique_ptr<D3D11::Renderer> mRenderer;

  D3D11::DeviceContextState mRenderState;

  using SwapchainBufferResources = D3D11::SwapchainBufferResources;
  using SwapchainResources = D3D11::SwapchainResources;

  std::unordered_map<XrSwapchain, SwapchainResources> mSwapchainResources;

  struct TrackedAppSwapchain {
    uint32_t mWidth {0};
    uint32_t mHeight {0};
    uint32_t mArraySize {1};
    uint32_t mSampleCount {1};
    DXGI_FORMAT mFormat {DXGI_FORMAT_UNKNOWN};
    std::vector<winrt::com_ptr<ID3D11Texture2D>> mImages;
    std::deque<uint32_t> mAcquiredQueue;
    uint32_t mLastReleasedIndex {0};
    bool mHasReleased {false};
  };

  std::mutex mAppSwapchainsMutex;
  std::unordered_map<uint64_t, TrackedAppSwapchain> mAppSwapchains;
  std::unordered_map<uint64_t, std::vector<winrt::com_ptr<ID3D11Texture2D>>>
    mOverlaySwapchainImages;

  uint32_t mMirrorRtWidth {0};
  uint32_t mMirrorRtHeight {0};
  winrt::com_ptr<ID3D11Texture2D> mMirrorRtTexture;
  winrt::com_ptr<ID3D11RenderTargetView> mMirrorRtView;
  std::array<winrt::com_ptr<ID3D11Texture2D>, 2> mMirrorStaging;
  std::array<bool, 2> mMirrorStagingReady {false, false};
  uint32_t mMirrorStagingWriteIdx {0};
  std::unique_ptr<DirectX::SpriteBatch> mMirrorSpriteBatch;
  std::unique_ptr<DirectX::CommonStates> mMirrorStates;

  winrt::com_ptr<ID3D11Texture2D> mMirrorScratch2D;
  uint32_t mMirrorScratchW {0};
  uint32_t mMirrorScratchH {0};
  DXGI_FORMAT mMirrorScratchFmt {DXGI_FORMAT_UNKNOWN};
  winrt::com_ptr<ID3D11ShaderResourceView> mMirrorScratchSRV;

  bool EnsureMirrorResources(uint32_t width, uint32_t height) noexcept;
  ID3D11ShaderResourceView* PrepareSourceSRV(
    ID3D11Texture2D* srcTex,
    uint32_t arrayIndex,
    DXGI_FORMAT declaredFormat) noexcept;
};

}// namespace VDXROVR
