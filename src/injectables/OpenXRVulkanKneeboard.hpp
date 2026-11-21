// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once
#define XR_USE_GRAPHICS_API_VULKAN

#include "OpenXRKneeboard.hpp"

#include <VDXROVR/SHM/Vulkan.hpp>
#include <VDXROVR/Vulkan.hpp>
#include <VDXROVR/Vulkan/ExtendedCreateInfo.hpp>
#include <VDXROVR/Vulkan/SpriteBatch.hpp>

#include <VDXROVR/config.hpp>

#include <shims/vulkan/vulkan.h>

#include <openxr/openxr_platform.h>

#include <d3d11_4.h>

#include <array>
#include <deque>
#include <mutex>
#include <vector>

namespace VDXROVR {

class OpenXRVulkanKneeboard final : public OpenXRKneeboard {
 public:
  using VkInstanceCreateInfo = Vulkan::CombinedCreateInfo<
    Vulkan::SpriteBatch::InstanceCreateInfo,
    SHM::Vulkan::InstanceCreateInfo>;
  using VkDeviceCreateInfo = Vulkan::CombinedCreateInfo<
    Vulkan::SpriteBatch::DeviceCreateInfo,
    SHM::Vulkan::DeviceCreateInfo>;

  OpenXRVulkanKneeboard() = delete;
  OpenXRVulkanKneeboard(
    XrInstance,
    XrSystemId,
    XrSession,
    OpenXRRuntimeID,
    const std::shared_ptr<OpenXRNext>&,
    const XrGraphicsBindingVulkanKHR&,
    const VkAllocationCallbacks*,
    PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr);
  ~OpenXRVulkanKneeboard();

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
    return VRMirrorGraphicsApi::Vulkan;
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
  std::unique_ptr<VDXROVR::Vulkan::Dispatch> mVK;

  const VkAllocationCallbacks* mAllocator {nullptr};
  VkInstance mVKInstance {VK_NULL_HANDLE};
  VkPhysicalDevice mPhysicalDevice {VK_NULL_HANDLE};
  VkDevice mDevice {VK_NULL_HANDLE};
  VkQueue mQueue {VK_NULL_HANDLE};
  uint32_t mQueueFamilyIndex {~(0ui32)};
  uint32_t mQueueIndex {~(0ui32)};

  std::unique_ptr<Vulkan::SpriteBatch> mSpriteBatch;

  template <class T>
  using unique_vk = Vulkan::unique_vk<T>;

  unique_vk<VkCommandPool> mCommandPool;
  std::vector<VkCommandBuffer> mCommandBuffers;

  struct SwapchainBufferResources {
    VkImage mImage {VK_NULL_HANDLE};
    unique_vk<VkImageView> mImageView;
    unique_vk<VkFence> mCompletionFence;
    VkCommandBuffer mCommandBuffer {VK_NULL_HANDLE};
  };

  struct SwapchainResources {
    XrSwapchain mSwapchain {XR_NULL_HANDLE};
    std::vector<SwapchainBufferResources> mBufferResources;
    PixelSize mDimensions;
  };

  std::optional<SwapchainResources> mSwapchainResources;

  std::unique_ptr<SHM::Vulkan::Reader> mSHM;

  struct TrackedAppSwapchain {
    uint32_t mWidth {0};
    uint32_t mHeight {0};
    uint32_t mArraySize {1};
    uint32_t mSampleCount {1};
    std::vector<VkImage> mImages;
    std::deque<uint32_t> mAcquiredQueue;
    uint32_t mLastReleasedIndex {0};
    bool mHasReleased {false};
  };

  struct MirrorVulkanSlot {
    VkCommandBuffer mCmdBuffer {VK_NULL_HANDLE};
    unique_vk<VkFence> mFence;
    unique_vk<VkImage> mBlitImage;
    unique_vk<VkDeviceMemory> mBlitMemory;
    unique_vk<VkBuffer> mReadbackBuffer;
    unique_vk<VkDeviceMemory> mReadbackMemory;
    uint32_t mWidth {0};
    uint32_t mHeight {0};
    bool mPending {false};
  };

  std::mutex mAppSwapchainsMutex;
  std::unordered_map<uint64_t, TrackedAppSwapchain> mAppSwapchains;
  unique_vk<VkCommandPool> mMirrorCommandPool;
  std::array<MirrorVulkanSlot, 2> mMirrorSlots;
  uint32_t mMirrorWriteIdx {0};

  uint32_t FindMemoryType(
    uint32_t typeBits,
    VkMemoryPropertyFlags requiredFlags) const noexcept;
  bool EnsureMirrorSlot(
    MirrorVulkanSlot& slot,
    uint32_t width,
    uint32_t height) noexcept;

  void WaitForAllFences();
};

}// namespace VDXROVR
