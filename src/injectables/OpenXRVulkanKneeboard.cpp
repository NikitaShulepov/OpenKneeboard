// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.

#include "OpenXRVulkanKneeboard.hpp"

#include "OpenXRNext.hpp"

#include <VDXROVR/dprint.hpp>
#include <VDXROVR/scope_exit.hpp>
#include <VDXROVR/tracing.hpp>

#include <shims/vulkan/vulkan.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include <d3d11_4.h>
#include <dxgi1_6.h>

template <class CharT>
struct std::formatter<VkResult, CharT> : std::formatter<int, CharT> {};

using VDXROVR::Vulkan::check_vkresult;

namespace VDXROVR {

OpenXRVulkanKneeboard::OpenXRVulkanKneeboard(
  XrInstance instance,
  XrSystemId systemID,
  XrSession session,
  OpenXRRuntimeID runtimeID,
  const std::shared_ptr<OpenXRNext>& next,
  const XrGraphicsBindingVulkanKHR& binding,
  const VkAllocationCallbacks* vulkanAllocator,
  PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr)
  : OpenXRKneeboard(instance, systemID, session, runtimeID, next) {
  dprint("{}", __FUNCTION__);
  TraceLoggingWrite(gTraceProvider, "OpenXRVulkanKneeboard()");

  mAllocator = vulkanAllocator;

  mVK = std::make_unique<Vulkan::Dispatch>(
    binding.instance, pfnVkGetInstanceProcAddr);
  mVKInstance = binding.instance;
  mPhysicalDevice = binding.physicalDevice;
  mDevice = binding.device;
  mQueueFamilyIndex = binding.queueFamilyIndex;
  mQueueIndex = binding.queueIndex;
  mVK->GetDeviceQueue(mDevice, mQueueFamilyIndex, mQueueIndex, &mQueue);

  mSpriteBatch = std::make_unique<Vulkan::SpriteBatch>(
    mVK.get(),
    mPhysicalDevice,
    mDevice,
    mAllocator,
    mQueueFamilyIndex,
    mQueueIndex);

  VkCommandPoolCreateInfo poolCreateInfo {
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
      | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    .queueFamilyIndex = mQueueFamilyIndex,
  };
  mCommandPool =
    mVK->make_unique<VkCommandPool>(mDevice, &poolCreateInfo, nullptr);

  mSHM = std::make_unique<SHM::Vulkan::Reader>(
    SHM::ConsumerKind::OpenXR_Vulkan2,
    mVK.get(),
    mVKInstance,
    mDevice,
    mPhysicalDevice,
    mQueue,
    mQueueFamilyIndex,
    mAllocator);
}

OpenXRVulkanKneeboard::~OpenXRVulkanKneeboard() {
  TraceLoggingWrite(gTraceProvider, "~OpenXRVulkanKneeboard()");
  this->WaitForAllFences();
}

XrSwapchain OpenXRVulkanKneeboard::CreateSwapchain(
  XrSession session,
  const PixelSize& size) {
  if (mSwapchainResources) [[unlikely]] {
    fatal("Asked to create a second swapchain");
  }

  static_assert(SHM::SHARED_TEXTURE_PIXEL_FORMAT == DXGI_FORMAT_B8G8R8A8_UNORM);
  const auto vkImageFormat = VK_FORMAT_B8G8R8A8_SRGB;
  const auto vkImageViewFormat = VK_FORMAT_B8G8R8A8_UNORM;
  XrSwapchainCreateInfo swapchainInfo {
    .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
    .usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT
      | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT
      | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT,
    .format = vkImageFormat,
    .sampleCount = 1,
    .width = size.mWidth,
    .height = size.mHeight,
    .faceCount = 1,
    .arraySize = 1,
    .mipCount = 1,
  };

  auto oxr = this->GetOpenXR();

  XrSwapchain swapchain {nullptr};
  {
    const auto ret =
      oxr->xrCreateSwapchain(session, &swapchainInfo, &swapchain);
    if (XR_FAILED(ret)) {
      dprint("next->xrCreateSwapchain failed: {}", ret);
      return nullptr;
    }
  }

  uint32_t imageCount = 0;
  {
    const auto res =
      oxr->xrEnumerateSwapchainImages(swapchain, 0, &imageCount, nullptr);
    if (XR_FAILED(res) || imageCount == 0) {
      dprint("No images in swapchain: {}", res);
      return nullptr;
    }
  }

  dprint("{} images in swapchain", imageCount);

  std::vector<XrSwapchainImageVulkanKHR> images;
  images.resize(
    imageCount,
    XrSwapchainImageVulkanKHR {
      .type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR,
    });
  {
    const auto res = oxr->xrEnumerateSwapchainImages(
      swapchain,
      imageCount,
      &imageCount,
      reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data()));
    if (XR_FAILED(res)) {
      dprint("Failed to enumerate images in swapchain: {}", res);
      oxr->xrDestroySwapchain(swapchain);
      return nullptr;
    }
  }

  if (images.at(0).type != XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR) {
    dprint("Swap chain is not a Vulkan swapchain");
    VDXROVR_BREAK;
    oxr->xrDestroySwapchain(swapchain);
    return nullptr;
  }

  if (imageCount > mCommandBuffers.size()) {
    const auto oldLength = mCommandBuffers.size();
    VkCommandBufferAllocateInfo allocInfo {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = mCommandPool.get(),
      .commandBufferCount = static_cast<uint32_t>(imageCount - oldLength),
    };
    mCommandBuffers.resize(imageCount);
    check_vkresult(mVK->AllocateCommandBuffers(
      mDevice, &allocInfo, &mCommandBuffers.at(oldLength)));
  }

  VkImageViewCreateInfo viewCreateInfo {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .viewType = VK_IMAGE_VIEW_TYPE_2D,
    .format = vkImageViewFormat,
    .subresourceRange =
      VkImageSubresourceRange {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
      },
  };
  VkFenceCreateInfo fenceCreateInfo {
    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    .flags = VK_FENCE_CREATE_SIGNALED_BIT,
  };

  std::vector<SwapchainBufferResources> buffers;
  for (auto& xrImage: images) {
    viewCreateInfo.image = xrImage.image;
    buffers.push_back(
      SwapchainBufferResources {
        .mImage = xrImage.image,
        .mImageView =
          mVK->make_unique<VkImageView>(mDevice, &viewCreateInfo, mAllocator),
        .mCompletionFence =
          mVK->make_unique<VkFence>(mDevice, &fenceCreateInfo, mAllocator),
        .mCommandBuffer = mCommandBuffers.at(buffers.size()),
      });
  }
  mSwapchainResources = {swapchain, std::move(buffers), size};

  return swapchain;
}

void OpenXRVulkanKneeboard::ReleaseSwapchainResources(XrSwapchain swapchain) {
  if (!(mSwapchainResources && mSwapchainResources->mSwapchain == swapchain))
    [[unlikely]] {
    fatal("Asked to destroy an inactive swapchain");
  }

  this->WaitForAllFences();

  mSwapchainResources = {};
}

void OpenXRVulkanKneeboard::WaitForAllFences() {
  std::vector<VkFence> fences;
  if (mSwapchainResources) {
    for (const auto& buffer: mSwapchainResources->mBufferResources) {
      fences.push_back(buffer.mCompletionFence.get());
    }
  }
  for (const auto& slot : mMirrorSlots) {
    if (slot.mFence && slot.mPending) {
      fences.push_back(slot.mFence.get());
    }
  }
  if (!fences.empty()) {
    mVK->WaitForFences(
      mDevice,
      static_cast<uint32_t>(fences.size()),
      fences.data(),
      VK_TRUE,
      1000000000ull);
  }
}

void OpenXRVulkanKneeboard::RenderLayers(
  XrSwapchain swapchain,
  uint32_t swapchainIndex,
  SHM::Frame rawFrame,
  const std::span<SHM::LayerSprite>& layers) {
  VDXROVR_TraceLoggingScope("OpenXRVulkanKneeboard::RenderLayers()");

  if (!(mSwapchainResources && mSwapchainResources->mSwapchain == swapchain))
    [[unlikely]] {
    fatal("Asked to render to wrong swapchain");
  }

  const auto& sr = *mSwapchainResources;
  const auto& br = sr.mBufferResources.at(swapchainIndex);

  auto dest = br.mImageView.get();
  auto cb = br.mCommandBuffer;

  const VkCommandBufferBeginInfo beginInfo {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  check_vkresult(mVK->BeginCommandBuffer(cb, &beginInfo));
  mSpriteBatch->Begin(cb, dest, sr.mDimensions);
  mSpriteBatch->Clear();

  auto frame = mSHM->Map(std::move(rawFrame));
  const auto baseTint = frame.mConfig.mTint;

  for (const auto& layer: layers) {
    const Vulkan::Color layerTint {
      baseTint[0] * layer.mOpacity,
      baseTint[1] * layer.mOpacity,
      baseTint[2] * layer.mOpacity,
      baseTint[3] * layer.mOpacity,
    };
    mSpriteBatch->Draw(
      frame.mImageView,
      frame.mDimensions,
      layer.mSourceRect,
      layer.mDestRect,
      layerTint);
  }

  mSpriteBatch->End();
  check_vkresult(mVK->EndCommandBuffer(cb));

  const auto fence = br.mCompletionFence.get();
  check_vkresult(mVK->ResetFences(mDevice, 1, &fence));

  const VkSemaphore waitSemaphores[] {frame.mSemaphore};
  const uint64_t waitSemaphoreValues[] {frame.mSemaphoreIn};

  const VkTimelineSemaphoreSubmitInfoKHR semaphoreInfo {
    .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO_KHR,
    .waitSemaphoreValueCount = std::size(waitSemaphoreValues),
    .pWaitSemaphoreValues = waitSemaphoreValues,
  };

  const VkPipelineStageFlags semaphoreStages {
    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT};

  const VkSubmitInfo submitInfo {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .pNext = &semaphoreInfo,
    .waitSemaphoreCount = std::size(waitSemaphores),
    .pWaitSemaphores = waitSemaphores,
    .pWaitDstStageMask = &semaphoreStages,
    .commandBufferCount = 1,
    .pCommandBuffers = &cb,
  };
  check_vkresult(mVK->QueueSubmit(mQueue, 1, &submitInfo, fence));
}

void OpenXRVulkanKneeboard::OnAppSwapchainCreated(
  const XrSwapchain swapchain,
  const XrSwapchainCreateInfo& createInfo) {
  if (
    !swapchain
    || (createInfo.usageFlags & XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT) == 0u) {
    return;
  }
  auto oxr = this->GetOpenXR();
  if (!oxr) {
    return;
  }
  uint32_t imageCount = 0u;
  if (XR_FAILED(oxr->xrEnumerateSwapchainImages(swapchain, 0u, &imageCount, nullptr))
      || imageCount == 0u) {
    return;
  }
  std::vector<XrSwapchainImageVulkanKHR> images(
    imageCount,
    XrSwapchainImageVulkanKHR {.type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR});
  if (XR_FAILED(oxr->xrEnumerateSwapchainImages(
        swapchain,
        imageCount,
        &imageCount,
        reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data())))) {
    return;
  }

  TrackedAppSwapchain tracked {};
  tracked.mWidth = createInfo.width;
  tracked.mHeight = createInfo.height;
  tracked.mArraySize = (std::max)(1u, createInfo.arraySize);
  tracked.mSampleCount = (std::max)(1u, createInfo.sampleCount);
  tracked.mImages.reserve(imageCount);
  for (const auto& img : images) {
    tracked.mImages.push_back(img.image);
  }

  std::scoped_lock lock(mAppSwapchainsMutex);
  mAppSwapchains[VRMirrorHandleToUInt64(swapchain)] = std::move(tracked);
}

void OpenXRVulkanKneeboard::OnAppSwapchainDestroyed(const XrSwapchain swapchain) {
  std::scoped_lock lock(mAppSwapchainsMutex);
  mAppSwapchains.erase(VRMirrorHandleToUInt64(swapchain));
}

void OpenXRVulkanKneeboard::OnAppSwapchainImageAcquired(
  const XrSwapchain swapchain,
  const uint32_t imageIndex) {
  std::scoped_lock lock(mAppSwapchainsMutex);
  if (auto it = mAppSwapchains.find(VRMirrorHandleToUInt64(swapchain));
      it != mAppSwapchains.end()) {
    it->second.mAcquiredQueue.push_back(imageIndex);
  }
}

void OpenXRVulkanKneeboard::OnAppSwapchainImageReleased(
  const XrSwapchain swapchain) {
  std::scoped_lock lock(mAppSwapchainsMutex);
  if (auto it = mAppSwapchains.find(VRMirrorHandleToUInt64(swapchain));
      it != mAppSwapchains.end()) {
    if (!it->second.mAcquiredQueue.empty()) {
      it->second.mLastReleasedIndex = it->second.mAcquiredQueue.front();
      it->second.mAcquiredQueue.pop_front();
      it->second.mHasReleased = true;
    }
  }
}

uint32_t OpenXRVulkanKneeboard::FindMemoryType(
  const uint32_t typeBits,
  const VkMemoryPropertyFlags requiredFlags) const noexcept {
  VkPhysicalDeviceMemoryProperties memProps {};
  mVK->GetPhysicalDeviceMemoryProperties(mPhysicalDevice, &memProps);
  for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
    if (
      (typeBits & (1u << i)) != 0u
      && (memProps.memoryTypes[i].propertyFlags & requiredFlags) == requiredFlags) {
      return i;
    }
  }
  return ~(0u);
}

bool OpenXRVulkanKneeboard::EnsureMirrorSlot(
  MirrorVulkanSlot& slot,
  const uint32_t width,
  const uint32_t height) noexcept {
  if (
    slot.mBlitImage && slot.mReadbackBuffer && slot.mFence
    && static_cast<bool>(slot.mCmdBuffer) && slot.mWidth == width
    && slot.mHeight == height) {
    return true;
  }

  try {
    if (!mMirrorCommandPool) {
      VkCommandPoolCreateInfo poolInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
          | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = mQueueFamilyIndex,
      };
      mMirrorCommandPool =
        mVK->make_unique<VkCommandPool>(mDevice, &poolInfo, mAllocator);
    }

    if (!slot.mCmdBuffer) {
      VkCommandBufferAllocateInfo allocInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = mMirrorCommandPool.get(),
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1u,
      };
      if (mVK->AllocateCommandBuffers(mDevice, &allocInfo, &slot.mCmdBuffer) != VK_SUCCESS) {
        return false;
      }
    }

    if (!slot.mFence) {
      VkFenceCreateInfo fenceInfo {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
      };
      slot.mFence = mVK->make_unique<VkFence>(mDevice, &fenceInfo, mAllocator);
    }

    slot.mBlitImage.reset();
    slot.mBlitMemory.reset();
    slot.mReadbackBuffer.reset();
    slot.mReadbackMemory.reset();

    VkImageCreateInfo imgInfo {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = VK_FORMAT_B8G8R8A8_UNORM,
      .extent = {width, height, 1u},
      .mipLevels = 1u,
      .arrayLayers = 1u,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    slot.mBlitImage = mVK->make_unique<VkImage>(mDevice, &imgInfo, mAllocator);

    VkMemoryRequirements imgReq {};
    mVK->GetImageMemoryRequirements(mDevice, slot.mBlitImage.get(), &imgReq);
    const uint32_t imgMemType =
      this->FindMemoryType(imgReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (imgMemType == ~(0u)) {
      return false;
    }
    VkMemoryAllocateInfo imgAlloc {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = imgReq.size,
      .memoryTypeIndex = imgMemType,
    };
    slot.mBlitMemory =
      mVK->make_unique<VkDeviceMemory>(mDevice, &imgAlloc, mAllocator);
    VkBindImageMemoryInfo bindImg {
      .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
      .image = slot.mBlitImage.get(),
      .memory = slot.mBlitMemory.get(),
      .memoryOffset = 0u,
    };
    if (mVK->BindImageMemory2KHR(mDevice, 1u, &bindImg) != VK_SUCCESS) {
      return false;
    }

    const VkDeviceSize bufBytes =
      static_cast<VkDeviceSize>(width) * static_cast<VkDeviceSize>(height) * 4u;
    VkBufferCreateInfo bufInfo {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = bufBytes,
      .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    slot.mReadbackBuffer =
      mVK->make_unique<VkBuffer>(mDevice, &bufInfo, mAllocator);

    VkMemoryRequirements bufReq {};
    mVK->GetBufferMemoryRequirements(mDevice, slot.mReadbackBuffer.get(), &bufReq);
    uint32_t bufMemType = this->FindMemoryType(
      bufReq.memoryTypeBits,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
    if (bufMemType == ~(0u)) {
      bufMemType = this->FindMemoryType(
        bufReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }
    if (bufMemType == ~(0u)) {
      return false;
    }
    VkMemoryAllocateInfo bufAlloc {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = bufReq.size,
      .memoryTypeIndex = bufMemType,
    };
    slot.mReadbackMemory =
      mVK->make_unique<VkDeviceMemory>(mDevice, &bufAlloc, mAllocator);
    if (mVK->BindBufferMemory(
          mDevice, slot.mReadbackBuffer.get(), slot.mReadbackMemory.get(), 0u)
        != VK_SUCCESS) {
      return false;
    }

    slot.mWidth = width;
    slot.mHeight = height;
    slot.mPending = false;
    return true;
  } catch (...) {
    return false;
  }
}

bool OpenXRVulkanKneeboard::CaptureMirrorFrame(
  const OpenXRMirror::CaptureRequest& request,
  std::vector<uint8_t>& outBgra,
  uint32_t& outWidth,
  uint32_t& outHeight,
  uint32_t& outSourceWidth,
  uint32_t& outSourceHeight) {
  outWidth = 0u;
  outHeight = 0u;
  outSourceWidth = 0u;
  outSourceHeight = 0u;

  if (!mVK || !mVK->CmdCopyImageToBuffer || !mVK->GetFenceStatus) {
    return false;
  }

  const auto lookupImage = [&](const XrSwapchain sc,
                               VkImage& img,
                               uint32_t& w,
                               uint32_t& h,
                               uint32_t& samples) {
    const auto it = mAppSwapchains.find(VRMirrorHandleToUInt64(sc));
    if (it != mAppSwapchains.end() && !it->second.mImages.empty()) {
      const uint32_t idx =
        it->second.mHasReleased ? it->second.mLastReleasedIndex : 0u;
      if (idx < it->second.mImages.size()) {
        img = it->second.mImages[idx];
        w = it->second.mWidth;
        h = it->second.mHeight;
        samples = it->second.mSampleCount;
      }
    }
  };

  VkImage leftImage = VK_NULL_HANDLE;
  VkImage rightImage = VK_NULL_HANDLE;
  uint32_t leftW = 0u, leftH = 0u, leftSamples = 1u;
  uint32_t rightW = 0u, rightH = 0u, rightSamples = 1u;
  {
    std::scoped_lock lock(mAppSwapchainsMutex);
    lookupImage(request.mLeftSwapchain, leftImage, leftW, leftH, leftSamples);
    if (request.mRightSwapchain) {
      lookupImage(request.mRightSwapchain, rightImage, rightW, rightH, rightSamples);
    }
  }
  if (!rightImage) {
    rightImage = leftImage;
    rightW = leftW;
    rightH = leftH;
    rightSamples = leftSamples;
  }
  if (!leftImage || leftSamples > 1u || leftW == 0u || leftH == 0u) {
    return false;
  }

  const bool isBoth = (request.mEyeMode == VRMirrorEyeMode::Both)
    && static_cast<bool>(rightImage) && (rightSamples <= 1u) && (rightW > 0u) && (rightH > 0u);
  const bool useRight = (request.mEyeMode == VRMirrorEyeMode::Right);
  VkImage srcImage = useRight ? rightImage : leftImage;
  uint32_t srcW = useRight ? rightW : leftW;
  uint32_t srcH = useRight ? rightH : leftH;
  uint32_t eyeSlice = useRight ? request.mRightArrayIndex : request.mLeftArrayIndex;
  XrRect2Di eyeRect = useRight ? request.mRightRect : request.mLeftRect;

  outSourceWidth = srcW;
  outSourceHeight = srcH;

  const uint32_t writeIdx = mMirrorWriteIdx & 1u;
  const uint32_t readIdx = writeIdx ^ 1u;
  auto& writeSlot = mMirrorSlots[writeIdx];
  auto& readSlot = mMirrorSlots[readIdx];

  bool producedFrame = false;
  if (
    readSlot.mPending && readSlot.mFence && readSlot.mReadbackMemory
    && mVK->GetFenceStatus(mDevice, readSlot.mFence.get()) == VK_SUCCESS) {
    const VkDeviceSize byteCount =
      static_cast<VkDeviceSize>(readSlot.mWidth) * readSlot.mHeight * 4u;
    void* mapped = nullptr;
    if (mVK->MapMemory(
          mDevice, readSlot.mReadbackMemory.get(), 0u, byteCount, 0u, &mapped)
          == VK_SUCCESS
        && mapped) {
      VkMappedMemoryRange invRange {
        .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
        .memory = readSlot.mReadbackMemory.get(),
        .offset = 0u,
        .size = VK_WHOLE_SIZE,
      };
      mVK->InvalidateMappedMemoryRanges(mDevice, 1u, &invRange);
      outBgra.resize(static_cast<size_t>(byteCount));
      std::memcpy(outBgra.data(), mapped, static_cast<size_t>(byteCount));
      mVK->UnmapMemory(mDevice, readSlot.mReadbackMemory.get());
      outWidth = readSlot.mWidth;
      outHeight = readSlot.mHeight;
      producedFrame = true;
    }
    readSlot.mPending = false;
  }

  const bool writeSlotReady = !writeSlot.mPending
    || (writeSlot.mFence
        && mVK->GetFenceStatus(mDevice, writeSlot.mFence.get()) == VK_SUCCESS);

  if (
    writeSlotReady
    && this->EnsureMirrorSlot(writeSlot, request.mTargetWidth, request.mTargetHeight)) {
    const auto computeCrop = [](const XrRect2Di& rect,
                                const uint32_t imgW,
                                const uint32_t imgH,
                                const float desiredAspect,
                                int32_t& outX,
                                int32_t& outY,
                                int32_t& outW,
                                int32_t& outH) {
      const int32_t rx = std::clamp(rect.offset.x, 0, static_cast<int32_t>(imgW));
      const int32_t ry = std::clamp(rect.offset.y, 0, static_cast<int32_t>(imgH));
      const int32_t rw = (rect.extent.width > 0)
        ? std::clamp(rect.extent.width, 1, static_cast<int32_t>(imgW) - rx)
        : static_cast<int32_t>(imgW) - rx;
      const int32_t rh = (rect.extent.height > 0)
        ? std::clamp(rect.extent.height, 1, static_cast<int32_t>(imgH) - ry)
        : static_cast<int32_t>(imgH) - ry;
      const float srcAspect = static_cast<float>(rw) / static_cast<float>(rh);
      outX = rx;
      outY = ry;
      outW = rw;
      outH = rh;
      if (srcAspect > desiredAspect) {
        outW = (std::max)(1, static_cast<int32_t>(std::lround(rh * desiredAspect)));
        outX = rx + ((rw - outW) / 2);
      } else if (srcAspect < desiredAspect) {
        outH = (std::max)(1, static_cast<int32_t>(std::lround(rw / desiredAspect)));
        outY = ry + ((rh - outH) / 2);
      }
    };

    VkCommandBuffer cb = writeSlot.mCmdBuffer;
    mVK->ResetCommandBuffer(cb, 0u);
    VkCommandBufferBeginInfo beginInfo {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    if (mVK->BeginCommandBuffer(cb, &beginInfo) == VK_SUCCESS) {
      const bool distinctRightSubresource = isBoth
        && (VRMirrorHandleToUInt64(rightImage) != VRMirrorHandleToUInt64(leftImage)
            || request.mRightArrayIndex != request.mLeftArrayIndex);

      VkImageMemoryBarrier preBarriers[3] {};
      uint32_t preCount = 2u;
      preBarriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      preBarriers[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      preBarriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      preBarriers[0].oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      preBarriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      preBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      preBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      preBarriers[0].image = isBoth ? leftImage : srcImage;
      preBarriers[0].subresourceRange = {
        VK_IMAGE_ASPECT_COLOR_BIT,
        0u,
        1u,
        isBoth ? request.mLeftArrayIndex : eyeSlice,
        1u};

      preBarriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      preBarriers[1].srcAccessMask = 0u;
      preBarriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      preBarriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      preBarriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      preBarriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      preBarriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      preBarriers[1].image = writeSlot.mBlitImage.get();
      preBarriers[1].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};

      if (distinctRightSubresource) {
        preBarriers[2] = preBarriers[0];
        preBarriers[2].image = rightImage;
        preBarriers[2].subresourceRange.baseArrayLayer = request.mRightArrayIndex;
        preCount = 3u;
      }

      mVK->CmdPipelineBarrier(
        cb,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0u,
        0u,
        nullptr,
        0u,
        nullptr,
        preCount,
        preBarriers);

      if (isBoth) {
        const int32_t halfW = static_cast<int32_t>(request.mTargetWidth / 2u);
        const float halfAspect =
          static_cast<float>(halfW) / static_cast<float>(request.mTargetHeight);
        int32_t lx = 0, ly = 0, lw = 0, lh = 0;
        computeCrop(request.mLeftRect, leftW, leftH, halfAspect, lx, ly, lw, lh);
        VkImageBlit leftBlit {};
        leftBlit.srcSubresource = {
          VK_IMAGE_ASPECT_COLOR_BIT, 0u, request.mLeftArrayIndex, 1u};
        leftBlit.srcOffsets[0] = {lx, ly, 0};
        leftBlit.srcOffsets[1] = {lx + lw, ly + lh, 1};
        leftBlit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
        leftBlit.dstOffsets[0] = {0, 0, 0};
        leftBlit.dstOffsets[1] = {halfW, static_cast<int32_t>(request.mTargetHeight), 1};
        mVK->CmdBlitImage(
          cb,
          leftImage,
          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
          writeSlot.mBlitImage.get(),
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          1u,
          &leftBlit,
          VK_FILTER_LINEAR);

        int32_t rx = 0, ry = 0, rw = 0, rh = 0;
        computeCrop(request.mRightRect, rightW, rightH, halfAspect, rx, ry, rw, rh);
        VkImageBlit rightBlit {};
        rightBlit.srcSubresource = {
          VK_IMAGE_ASPECT_COLOR_BIT, 0u, request.mRightArrayIndex, 1u};
        rightBlit.srcOffsets[0] = {rx, ry, 0};
        rightBlit.srcOffsets[1] = {rx + rw, ry + rh, 1};
        rightBlit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
        rightBlit.dstOffsets[0] = {halfW, 0, 0};
        rightBlit.dstOffsets[1] = {
          static_cast<int32_t>(request.mTargetWidth),
          static_cast<int32_t>(request.mTargetHeight),
          1};
        mVK->CmdBlitImage(
          cb,
          rightImage,
          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
          writeSlot.mBlitImage.get(),
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          1u,
          &rightBlit,
          VK_FILTER_LINEAR);
      } else {
        const float targetAspect = static_cast<float>(request.mTargetWidth)
          / static_cast<float>(request.mTargetHeight);
        int32_t cropX = 0, cropY = 0, cropW = 0, cropH = 0;
        computeCrop(eyeRect, srcW, srcH, targetAspect, cropX, cropY, cropW, cropH);
        VkImageBlit blit {};
        blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, eyeSlice, 1u};
        blit.srcOffsets[0] = {cropX, cropY, 0};
        blit.srcOffsets[1] = {cropX + cropW, cropY + cropH, 1};
        blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {
          static_cast<int32_t>(request.mTargetWidth),
          static_cast<int32_t>(request.mTargetHeight),
          1};
        mVK->CmdBlitImage(
          cb,
          srcImage,
          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
          writeSlot.mBlitImage.get(),
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          1u,
          &blit,
          VK_FILTER_LINEAR);
      }

      VkImageMemoryBarrier midBarriers[3] {};
      midBarriers[0] = preBarriers[0];
      midBarriers[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      midBarriers[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      midBarriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      midBarriers[0].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

      midBarriers[1] = preBarriers[1];
      midBarriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      midBarriers[1].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      midBarriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      midBarriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

      if (distinctRightSubresource) {
        midBarriers[2] = preBarriers[2];
        midBarriers[2].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        midBarriers[2].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        midBarriers[2].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        midBarriers[2].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      }

      mVK->CmdPipelineBarrier(
        cb,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0u,
        0u,
        nullptr,
        0u,
        nullptr,
        preCount,
        midBarriers);

      VkBufferImageCopy copyRegion {};
      copyRegion.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
      copyRegion.imageExtent = {request.mTargetWidth, request.mTargetHeight, 1u};
      mVK->CmdCopyImageToBuffer(
        cb,
        writeSlot.mBlitImage.get(),
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        writeSlot.mReadbackBuffer.get(),
        1u,
        &copyRegion);

      if (mVK->EndCommandBuffer(cb) == VK_SUCCESS) {
        const VkFence fence = writeSlot.mFence.get();
        mVK->ResetFences(mDevice, 1u, &fence);
        VkSubmitInfo submitInfo {
          .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
          .commandBufferCount = 1u,
          .pCommandBuffers = &cb,
        };
        if (mVK->QueueSubmit(mQueue, 1u, &submitInfo, fence) == VK_SUCCESS) {
          writeSlot.mPending = true;
          mMirrorWriteIdx = readIdx;
        }
      }
    }
  }

  return producedFrame;
}

SHM::Reader& OpenXRVulkanKneeboard::GetSHM() { return *mSHM; }

}// namespace VDXROVR
