// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include "OpenXRKneeboard.hpp"

#include <VDXROVR/D3D12.hpp>
#include <VDXROVR/D3D12/Renderer.hpp>
#include <VDXROVR/SHM/D3D12.hpp>

#include <VDXROVR/config.hpp>

#include <d3d12.h>

#include <directxtk12/DescriptorHeap.h>
#include <directxtk12/GraphicsMemory.h>

#include <array>
#include <deque>
#include <mutex>
#include <vector>

struct XrGraphicsBindingD3D12KHR;

namespace VDXROVR {

class OpenXRD3D12Kneeboard final : public OpenXRKneeboard {
 public:
  OpenXRD3D12Kneeboard() = delete;
  OpenXRD3D12Kneeboard(
    XrInstance,
    XrSystemId,
    XrSession,
    OpenXRRuntimeID,
    const std::shared_ptr<OpenXRNext>&,
    const XrGraphicsBindingD3D12KHR&);
  ~OpenXRD3D12Kneeboard();

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
    return VRMirrorGraphicsApi::D3D12;
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
  std::unique_ptr<SHM::D3D12::Reader> mSHM;

  std::unique_ptr<DirectX::GraphicsMemory> mGraphicsMemory;

  winrt::com_ptr<ID3D12Device> mDevice;
  winrt::com_ptr<ID3D12CommandQueue> mCommandQueue;

  using SwapchainBufferResources = D3D12::SwapchainBufferResources;
  using SwapchainResources = D3D12::SwapchainResources;

  std::unordered_map<XrSwapchain, SwapchainResources> mSwapchainResources;
  std::unique_ptr<D3D12::Renderer> mRenderer;

  struct TrackedAppSwapchain {
    uint32_t mWidth {0};
    uint32_t mHeight {0};
    uint32_t mArraySize {1};
    DXGI_FORMAT mFormat {DXGI_FORMAT_UNKNOWN};
    std::vector<winrt::com_ptr<ID3D12Resource>> mImages;
    std::deque<uint32_t> mAcquiredQueue;
    uint32_t mLastReleasedIndex {0};
    bool mHasReleased {false};
  };

  struct MirrorReadbackSlot {
    winrt::com_ptr<ID3D12CommandAllocator> mAllocator;
    winrt::com_ptr<ID3D12Resource> mReadbackBuffer;
    winrt::com_ptr<ID3D12Resource> mRightReadbackBuffer;
    uint64_t mBufferBytes {0};
    uint64_t mRightBufferBytes {0};
    uint64_t mFenceValue {0};
    uint32_t mSrcWidth {0};
    uint32_t mSrcHeight {0};
    uint32_t mRowPitch {0};
    DXGI_FORMAT mFormat {DXGI_FORMAT_UNKNOWN};
    XrRect2Di mCropRect {};
    uint32_t mRightSrcWidth {0};
    uint32_t mRightSrcHeight {0};
    uint32_t mRightRowPitch {0};
    DXGI_FORMAT mRightFormat {DXGI_FORMAT_UNKNOWN};
    XrRect2Di mRightCropRect {};
    bool mRightSameBuffer {false};
    VRMirrorEyeMode mEyeMode {VRMirrorEyeMode::Left};
    uint32_t mTargetWidth {0};
    uint32_t mTargetHeight {0};
    bool mPending {false};
  };

  std::mutex mAppSwapchainsMutex;
  std::unordered_map<uint64_t, TrackedAppSwapchain> mAppSwapchains;
  winrt::com_ptr<ID3D12GraphicsCommandList> mMirrorCmdList;
  winrt::com_ptr<ID3D12Fence> mMirrorFence;
  HANDLE mMirrorFenceEvent {nullptr};
  uint64_t mMirrorFenceCounter {0};
  std::array<MirrorReadbackSlot, 2> mMirrorSlots;
  uint32_t mMirrorWriteIdx {0};

  bool EnsureMirrorReadbackSlot(
    MirrorReadbackSlot& slot,
    uint64_t requiredBytes) noexcept;
};

}// namespace VDXROVR
