// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.

#include "OpenXRD3D12Kneeboard.hpp"

#include "OpenXRD3D11Kneeboard.hpp"
#include "OpenXRNext.hpp"

#include <VDXROVR/config.hpp>
#include <VDXROVR/dprint.hpp>
#include <VDXROVR/tracing.hpp>

#include <shims/winrt/base.h>

#include <d3d11.h>
#include <d3d12.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

#include <d3d11on12.h>

#include <directxtk12/RenderTargetState.h>
#include <directxtk12/ResourceUploadBatch.h>

#define XR_USE_GRAPHICS_API_D3D12
#include <openxr/openxr_platform.h>

namespace VDXROVR {

OpenXRD3D12Kneeboard::OpenXRD3D12Kneeboard(
  XrInstance instance,
  XrSystemId systemID,
  XrSession session,
  OpenXRRuntimeID runtimeID,
  const std::shared_ptr<OpenXRNext>& next,
  const XrGraphicsBindingD3D12KHR& binding)
  : OpenXRKneeboard(instance, systemID, session, runtimeID, next) {
  dprint("{}", __FUNCTION__);

  mGraphicsMemory = std::make_unique<DirectX::GraphicsMemory>(binding.device);

  mDevice.copy_from(binding.device);
  mCommandQueue.copy_from(binding.queue);
  mRenderer = std::make_unique<D3D12::Renderer>(
    mDevice.get(), mCommandQueue.get(), DXGI_FORMAT_B8G8R8A8_UNORM);

  mSHM = std::make_unique<SHM::D3D12::Reader>(
    SHM::ConsumerKind::OpenXR_D3D12, mDevice.get(), mCommandQueue.get());
}

OpenXRD3D12Kneeboard::~OpenXRD3D12Kneeboard() {
  TraceLoggingWrite(gTraceProvider, "~OpenXRD3D12Kneeboard()");
  if (mMirrorFence && mMirrorFenceEvent && mMirrorFenceCounter > 0u) {
    if (mMirrorFence->GetCompletedValue() < mMirrorFenceCounter) {
      if (SUCCEEDED(mMirrorFence->SetEventOnCompletion(
            mMirrorFenceCounter, mMirrorFenceEvent))) {
        WaitForSingleObject(mMirrorFenceEvent, 1000u);
      }
    }
  }
  if (mMirrorFenceEvent) {
    CloseHandle(mMirrorFenceEvent);
    mMirrorFenceEvent = nullptr;
  }
}

XrSwapchain OpenXRD3D12Kneeboard::CreateSwapchain(
  XrSession session,
  const PixelSize& size) {
  dprint("{}", __FUNCTION__);
  VDXROVR_TraceLoggingScope("OpenXRD3D12Kneeboard::CreateSwapchain");

  auto oxr = this->GetOpenXR();

  auto formats = OpenXRD3D11Kneeboard::GetDXGIFormats(oxr, session);
  dprint(
    "Creating swapchain with format {}",
    static_cast<INT>(formats.mTextureFormat));

  XrSwapchainCreateInfo swapchainInfo {
    .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
    .usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT,
    .format = formats.mTextureFormat,
    .sampleCount = 1,
    .width = size.mWidth,
    .height = size.mHeight,
    .faceCount = 1,
    .arraySize = 1,
    .mipCount = 1,
  };

  XrSwapchain swapchain {nullptr};

  auto nextResult = oxr->xrCreateSwapchain(session, &swapchainInfo, &swapchain);
  if (nextResult != XR_SUCCESS) {
    dprint("Failed to create swapchain: {}", nextResult);
    return nullptr;
  }

  uint32_t imageCount = 0;
  nextResult =
    oxr->xrEnumerateSwapchainImages(swapchain, 0, &imageCount, nullptr);
  if (imageCount == 0 || nextResult != XR_SUCCESS) {
    dprint("No images in swapchain: {}", nextResult);
    return nullptr;
  }

  dprint("{} images in swapchain", imageCount);

  std::vector<XrSwapchainImageD3D12KHR> images;
  images.resize(
    imageCount,
    XrSwapchainImageD3D12KHR {
      .type = XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR,
    });

  nextResult = oxr->xrEnumerateSwapchainImages(
    swapchain,
    imageCount,
    &imageCount,
    reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data()));
  if (nextResult != XR_SUCCESS) {
    dprint("Failed to enumerate images in swapchain: {}", nextResult);
    oxr->xrDestroySwapchain(swapchain);
    return nullptr;
  }

  if (images.at(0).type != XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR) {
    dprint("Swap chain is not a D3D12 swapchain");
    VDXROVR_BREAK;
    oxr->xrDestroySwapchain(swapchain);
    return nullptr;
  }

  this->ReleaseSwapchainResources(swapchain);
  mSwapchainResources.emplace(
    swapchain,
    SwapchainResources {
      .mDimensions = size,
      .mRenderTargetViewHeap =
        {mDevice.get(),
         D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
         D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
         imageCount},
    });

  static std::atomic<uint64_t> sSwapChainCount;
  const auto thisSwapchain = sSwapChainCount++;

  auto& sr = mSwapchainResources.at(swapchain);
  for (const auto& image: images) {
    const auto imageIndex = sr.mBufferResources.size();
    sr.mBufferResources.push_back({
      mDevice.get(),
      image.texture,
      sr.mRenderTargetViewHeap.GetCpuHandle(imageIndex),
      formats.mRenderTargetViewFormat,
    });
    image.texture->SetName(
      std::format(
        L"VDXROVR D3D12 OpenXR swapchain #{} subimage #{}",
        thisSwapchain,
        imageIndex)
        .c_str());
  }

  return swapchain;
}

void OpenXRD3D12Kneeboard::ReleaseSwapchainResources(XrSwapchain swapchain) {
  if (!mSwapchainResources.contains(swapchain)) {
    return;
  }
  mSwapchainResources.erase(swapchain);
}

void OpenXRD3D12Kneeboard::RenderLayers(
  XrSwapchain swapchain,
  uint32_t swapchainTextureIndex,
  SHM::Frame frame,
  const std::span<SHM::LayerSprite>& layers) {
  VDXROVR_TraceLoggingScope("OpenXRD3D12Kneeboard::RenderLayers()");

  mRenderer->RenderLayers(
    mSwapchainResources.at(swapchain),
    swapchainTextureIndex,
    mSHM->Map(std::move(frame)),
    layers,
    RenderMode::ClearAndRender);
  mGraphicsMemory->Commit(mCommandQueue.get());
}

namespace {

void ConvertDownscaleRegionToBGRA8(
  const uint8_t* srcData,
  const uint32_t srcW,
  const uint32_t srcH,
  const uint32_t rowPitch,
  const DXGI_FORMAT fmt,
  const XrRect2Di& eyeRect,
  uint8_t* outData,
  const uint32_t outStridePixels,
  const uint32_t dstOffsetX,
  const uint32_t dstRegionW,
  const uint32_t dstH) {
  if (
    !srcData || !outData || srcW == 0u || srcH == 0u || dstRegionW == 0u
    || dstH == 0u) {
    return;
  }

  const int32_t rx = std::clamp(eyeRect.offset.x, 0, static_cast<int32_t>(srcW));
  const int32_t ry = std::clamp(eyeRect.offset.y, 0, static_cast<int32_t>(srcH));
  const int32_t rw = (eyeRect.extent.width > 0)
    ? std::clamp(eyeRect.extent.width, 1, static_cast<int32_t>(srcW) - rx)
    : static_cast<int32_t>(srcW) - rx;
  const int32_t rh = (eyeRect.extent.height > 0)
    ? std::clamp(eyeRect.extent.height, 1, static_cast<int32_t>(srcH) - ry)
    : static_cast<int32_t>(srcH) - ry;

  const float targetAspect =
    static_cast<float>(dstRegionW) / static_cast<float>(dstH);
  const float srcAspect = static_cast<float>(rw) / static_cast<float>(rh);
  int32_t cropX = rx;
  int32_t cropY = ry;
  int32_t cropW = rw;
  int32_t cropH = rh;
  if (srcAspect > targetAspect) {
    cropW = (std::max)(1, static_cast<int32_t>(std::lround(rh * targetAspect)));
    cropX = rx + ((rw - cropW) / 2);
  } else if (srcAspect < targetAspect) {
    cropH = (std::max)(1, static_cast<int32_t>(std::lround(rw / targetAspect)));
    cropY = ry + ((rh - cropH) / 2);
  }

  const bool isRgba8 = (fmt == DXGI_FORMAT_R8G8B8A8_UNORM
    || fmt == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
    || fmt == DXGI_FORMAT_R8G8B8A8_TYPELESS);
  const bool isR10G10B10A2 = (fmt == DXGI_FORMAT_R10G10B10A2_UNORM);

  for (uint32_t y = 0u; y < dstH; ++y) {
    const uint32_t sy = static_cast<uint32_t>(
      cropY + static_cast<int32_t>((static_cast<uint64_t>(y) * cropH) / dstH));
    const uint8_t* srcRow = srcData + static_cast<size_t>(sy) * rowPitch;
    uint8_t* dstRow =
      outData + (static_cast<size_t>(y) * outStridePixels + dstOffsetX) * 4u;
    for (uint32_t x = 0u; x < dstRegionW; ++x) {
      const uint32_t sx = static_cast<uint32_t>(
        cropX + static_cast<int32_t>((static_cast<uint64_t>(x) * cropW) / dstRegionW));
      uint8_t b = 0u;
      uint8_t g = 0u;
      uint8_t r = 0u;
      if (isR10G10B10A2) {
        const uint32_t p =
          *reinterpret_cast<const uint32_t*>(srcRow + static_cast<size_t>(sx) * 4u);
        r = static_cast<uint8_t>((p & 0x3FFu) >> 2);
        g = static_cast<uint8_t>(((p >> 10) & 0x3FFu) >> 2);
        b = static_cast<uint8_t>(((p >> 20) & 0x3FFu) >> 2);
      } else if (isRgba8) {
        const uint8_t* px = srcRow + static_cast<size_t>(sx) * 4u;
        r = px[0];
        g = px[1];
        b = px[2];
      } else {
        const uint8_t* px = srcRow + static_cast<size_t>(sx) * 4u;
        b = px[0];
        g = px[1];
        r = px[2];
      }
      uint8_t* out = dstRow + static_cast<size_t>(x) * 4u;
      out[0] = b;
      out[1] = g;
      out[2] = r;
      out[3] = 255u;
    }
  }
}

}// namespace

void OpenXRD3D12Kneeboard::OnAppSwapchainCreated(
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
  std::vector<XrSwapchainImageD3D12KHR> images(
    imageCount,
    XrSwapchainImageD3D12KHR {.type = XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR});
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
  tracked.mFormat = static_cast<DXGI_FORMAT>(createInfo.format);
  tracked.mImages.reserve(imageCount);
  for (const auto& img : images) {
    winrt::com_ptr<ID3D12Resource> ptr;
    ptr.copy_from(img.texture);
    tracked.mImages.push_back(std::move(ptr));
  }

  std::scoped_lock lock(mAppSwapchainsMutex);
  mAppSwapchains[VRMirrorHandleToUInt64(swapchain)] = std::move(tracked);
}

void OpenXRD3D12Kneeboard::OnAppSwapchainDestroyed(const XrSwapchain swapchain) {
  std::scoped_lock lock(mAppSwapchainsMutex);
  mAppSwapchains.erase(VRMirrorHandleToUInt64(swapchain));
}

void OpenXRD3D12Kneeboard::OnAppSwapchainImageAcquired(
  const XrSwapchain swapchain,
  const uint32_t imageIndex) {
  std::scoped_lock lock(mAppSwapchainsMutex);
  if (auto it = mAppSwapchains.find(VRMirrorHandleToUInt64(swapchain));
      it != mAppSwapchains.end()) {
    it->second.mAcquiredQueue.push_back(imageIndex);
  }
}

void OpenXRD3D12Kneeboard::OnAppSwapchainImageReleased(
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

bool OpenXRD3D12Kneeboard::EnsureMirrorReadbackSlot(
  MirrorReadbackSlot& slot,
  const uint64_t requiredBytes) noexcept {
  if (!mMirrorFence) {
    if (FAILED(mDevice->CreateFence(
          0u, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(mMirrorFence.put())))) {
      return false;
    }
    mMirrorFenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  }
  if (!slot.mAllocator) {
    if (FAILED(mDevice->CreateCommandAllocator(
          D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(slot.mAllocator.put())))) {
      return false;
    }
  }
  if (!mMirrorCmdList) {
    if (FAILED(mDevice->CreateCommandList(
          0u,
          D3D12_COMMAND_LIST_TYPE_DIRECT,
          slot.mAllocator.get(),
          nullptr,
          IID_PPV_ARGS(mMirrorCmdList.put())))) {
      return false;
    }
    mMirrorCmdList->Close();
  }
  if (!slot.mReadbackBuffer || slot.mBufferBytes < requiredBytes) {
    slot.mReadbackBuffer = nullptr;
    D3D12_HEAP_PROPERTIES heapProps {};
    heapProps.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC bufDesc {};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width = requiredBytes;
    bufDesc.Height = 1u;
    bufDesc.DepthOrArraySize = 1u;
    bufDesc.MipLevels = 1u;
    bufDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufDesc.SampleDesc = {1u, 0u};
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    if (FAILED(mDevice->CreateCommittedResource(
          &heapProps,
          D3D12_HEAP_FLAG_NONE,
          &bufDesc,
          D3D12_RESOURCE_STATE_COPY_DEST,
          nullptr,
          IID_PPV_ARGS(slot.mReadbackBuffer.put())))) {
      return false;
    }
    slot.mBufferBytes = requiredBytes;
  }
  return true;
}

bool OpenXRD3D12Kneeboard::CaptureMirrorFrame(
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

  const auto lookupRes = [&](const XrSwapchain sc,
                             winrt::com_ptr<ID3D12Resource>& outRes,
                             DXGI_FORMAT& outFmt) {
    const auto it = mAppSwapchains.find(VRMirrorHandleToUInt64(sc));
    if (it != mAppSwapchains.end() && !it->second.mImages.empty()) {
      const uint32_t idx =
        it->second.mHasReleased ? it->second.mLastReleasedIndex : 0u;
      if (idx < it->second.mImages.size()) {
        outRes = it->second.mImages[idx];
        outFmt = it->second.mFormat;
      }
    }
  };

  winrt::com_ptr<ID3D12Resource> leftRes;
  winrt::com_ptr<ID3D12Resource> rightRes;
  DXGI_FORMAT leftFmt = DXGI_FORMAT_UNKNOWN;
  DXGI_FORMAT rightFmt = DXGI_FORMAT_UNKNOWN;
  {
    std::scoped_lock lock(mAppSwapchainsMutex);
    lookupRes(request.mLeftSwapchain, leftRes, leftFmt);
    if (request.mRightSwapchain) {
      lookupRes(request.mRightSwapchain, rightRes, rightFmt);
    }
  }
  if (!rightRes) {
    rightRes = leftRes;
    rightFmt = leftFmt;
  }
  if (!leftRes) {
    return false;
  }

  const bool isBoth = (request.mEyeMode == VRMirrorEyeMode::Both) && rightRes;
  const bool useRight = (request.mEyeMode == VRMirrorEyeMode::Right);
  ID3D12Resource* primaryRes = (isBoth || !useRight) ? leftRes.get() : rightRes.get();
  const DXGI_FORMAT primaryFmt = (isBoth || !useRight) ? leftFmt : rightFmt;
  const uint32_t primarySlice =
    (isBoth || !useRight) ? request.mLeftArrayIndex : request.mRightArrayIndex;
  const XrRect2Di primaryRect =
    (isBoth || !useRight) ? request.mLeftRect : request.mRightRect;

  const D3D12_RESOURCE_DESC resDesc = primaryRes->GetDesc();
  if (resDesc.SampleDesc.Count > 1u) {
    return false;
  }

  outSourceWidth = static_cast<uint32_t>(resDesc.Width);
  outSourceHeight = resDesc.Height;

  const uint32_t subresource =
    (std::min)(primarySlice, static_cast<uint32_t>(resDesc.DepthOrArraySize - 1u))
    * resDesc.MipLevels;
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint {};
  UINT numRows = 0u;
  UINT64 rowSize = 0u;
  UINT64 totalBytes = 0u;
  mDevice->GetCopyableFootprints(
    &resDesc, subresource, 1u, 0u, &footprint, &numRows, &rowSize, &totalBytes);

  D3D12_RESOURCE_DESC rightDesc {};
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT rightFootprint {};
  UINT64 rightTotalBytes = 0u;
  uint32_t rightSubresource = 0u;
  bool rightSameBuffer = false;
  if (isBoth) {
    rightDesc = rightRes->GetDesc();
    if (rightDesc.SampleDesc.Count > 1u) {
      return false;
    }
    rightSubresource =
      (std::min)(
        request.mRightArrayIndex,
        static_cast<uint32_t>(rightDesc.DepthOrArraySize - 1u))
      * rightDesc.MipLevels;
    rightSameBuffer =
      (rightRes.get() == leftRes.get() && rightSubresource == subresource);
    if (!rightSameBuffer) {
      UINT rRows = 0u;
      UINT64 rRowSize = 0u;
      mDevice->GetCopyableFootprints(
        &rightDesc,
        rightSubresource,
        1u,
        0u,
        &rightFootprint,
        &rRows,
        &rRowSize,
        &rightTotalBytes);
    }
  }

  const uint32_t writeIdx = mMirrorWriteIdx & 1u;
  const uint32_t readIdx = writeIdx ^ 1u;
  auto& writeSlot = mMirrorSlots[writeIdx];
  auto& readSlot = mMirrorSlots[readIdx];

  bool producedFrame = false;
  if (
    readSlot.mPending && mMirrorFence
    && mMirrorFence->GetCompletedValue() >= readSlot.mFenceValue
    && readSlot.mReadbackBuffer) {
    void* mapped = nullptr;
    const D3D12_RANGE readRange {0u, static_cast<SIZE_T>(readSlot.mBufferBytes)};
    if (SUCCEEDED(readSlot.mReadbackBuffer->Map(0u, &readRange, &mapped)) && mapped) {
      outBgra.resize(
        static_cast<size_t>(readSlot.mTargetWidth) * readSlot.mTargetHeight * 4u);
      if (readSlot.mEyeMode == VRMirrorEyeMode::Both) {
        const uint32_t halfW = readSlot.mTargetWidth / 2u;
        ConvertDownscaleRegionToBGRA8(
          static_cast<const uint8_t*>(mapped),
          readSlot.mSrcWidth,
          readSlot.mSrcHeight,
          readSlot.mRowPitch,
          readSlot.mFormat,
          readSlot.mCropRect,
          outBgra.data(),
          readSlot.mTargetWidth,
          0u,
          halfW,
          readSlot.mTargetHeight);
        if (readSlot.mRightSameBuffer) {
          ConvertDownscaleRegionToBGRA8(
            static_cast<const uint8_t*>(mapped),
            readSlot.mSrcWidth,
            readSlot.mSrcHeight,
            readSlot.mRowPitch,
            readSlot.mFormat,
            readSlot.mRightCropRect,
            outBgra.data(),
            readSlot.mTargetWidth,
            halfW,
            readSlot.mTargetWidth - halfW,
            readSlot.mTargetHeight);
        } else if (readSlot.mRightReadbackBuffer) {
          void* mappedR = nullptr;
          const D3D12_RANGE rRange {
            0u, static_cast<SIZE_T>(readSlot.mRightBufferBytes)};
          if (
            SUCCEEDED(readSlot.mRightReadbackBuffer->Map(0u, &rRange, &mappedR))
            && mappedR) {
            ConvertDownscaleRegionToBGRA8(
              static_cast<const uint8_t*>(mappedR),
              readSlot.mRightSrcWidth,
              readSlot.mRightSrcHeight,
              readSlot.mRightRowPitch,
              readSlot.mRightFormat,
              readSlot.mRightCropRect,
              outBgra.data(),
              readSlot.mTargetWidth,
              halfW,
              readSlot.mTargetWidth - halfW,
              readSlot.mTargetHeight);
            const D3D12_RANGE writeRange {0u, 0u};
            readSlot.mRightReadbackBuffer->Unmap(0u, &writeRange);
          }
        }
      } else {
        ConvertDownscaleRegionToBGRA8(
          static_cast<const uint8_t*>(mapped),
          readSlot.mSrcWidth,
          readSlot.mSrcHeight,
          readSlot.mRowPitch,
          readSlot.mFormat,
          readSlot.mCropRect,
          outBgra.data(),
          readSlot.mTargetWidth,
          0u,
          readSlot.mTargetWidth,
          readSlot.mTargetHeight);
      }
      const D3D12_RANGE writeRange {0u, 0u};
      readSlot.mReadbackBuffer->Unmap(0u, &writeRange);
      outWidth = readSlot.mTargetWidth;
      outHeight = readSlot.mTargetHeight;
      producedFrame = true;
    }
    readSlot.mPending = false;
  }

  const auto ensureRightBuffer = [&](MirrorReadbackSlot& slot, const uint64_t bytes) {
    if (!isBoth || rightSameBuffer) {
      return true;
    }
    if (slot.mRightReadbackBuffer && slot.mRightBufferBytes >= bytes) {
      return true;
    }
    slot.mRightReadbackBuffer = nullptr;
    D3D12_HEAP_PROPERTIES heapProps {};
    heapProps.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC bufDesc {};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width = bytes;
    bufDesc.Height = 1u;
    bufDesc.DepthOrArraySize = 1u;
    bufDesc.MipLevels = 1u;
    bufDesc.SampleDesc = {1u, 0u};
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    if (FAILED(mDevice->CreateCommittedResource(
          &heapProps,
          D3D12_HEAP_FLAG_NONE,
          &bufDesc,
          D3D12_RESOURCE_STATE_COPY_DEST,
          nullptr,
          IID_PPV_ARGS(slot.mRightReadbackBuffer.put())))) {
      return false;
    }
    slot.mRightBufferBytes = bytes;
    return true;
  };

  if (
    (!writeSlot.mPending
     || (mMirrorFence && mMirrorFence->GetCompletedValue() >= writeSlot.mFenceValue))
    && this->EnsureMirrorReadbackSlot(writeSlot, totalBytes)
    && ensureRightBuffer(writeSlot, rightTotalBytes)) {
    if (
      SUCCEEDED(writeSlot.mAllocator->Reset())
      && SUCCEEDED(mMirrorCmdList->Reset(writeSlot.mAllocator.get(), nullptr))) {
      D3D12_RESOURCE_BARRIER toCopy[2] {};
      UINT barrierCount = 1u;
      toCopy[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      toCopy[0].Transition.pResource = primaryRes;
      toCopy[0].Transition.Subresource = subresource;
      toCopy[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
      toCopy[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
      if (isBoth && !rightSameBuffer) {
        toCopy[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        toCopy[1].Transition.pResource = rightRes.get();
        toCopy[1].Transition.Subresource = rightSubresource;
        toCopy[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
        toCopy[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        barrierCount = 2u;
      }
      mMirrorCmdList->ResourceBarrier(barrierCount, toCopy);

      D3D12_TEXTURE_COPY_LOCATION srcLoc {};
      srcLoc.pResource = primaryRes;
      srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
      srcLoc.SubresourceIndex = subresource;

      D3D12_TEXTURE_COPY_LOCATION dstLoc {};
      dstLoc.pResource = writeSlot.mReadbackBuffer.get();
      dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
      dstLoc.PlacedFootprint = footprint;
      mMirrorCmdList->CopyTextureRegion(&dstLoc, 0u, 0u, 0u, &srcLoc, nullptr);

      if (isBoth && !rightSameBuffer) {
        D3D12_TEXTURE_COPY_LOCATION srcR {};
        srcR.pResource = rightRes.get();
        srcR.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        srcR.SubresourceIndex = rightSubresource;
        D3D12_TEXTURE_COPY_LOCATION dstR {};
        dstR.pResource = writeSlot.mRightReadbackBuffer.get();
        dstR.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        dstR.PlacedFootprint = rightFootprint;
        mMirrorCmdList->CopyTextureRegion(&dstR, 0u, 0u, 0u, &srcR, nullptr);
      }

      for (UINT i = 0u; i < barrierCount; ++i) {
        std::swap(
          toCopy[i].Transition.StateBefore, toCopy[i].Transition.StateAfter);
      }
      mMirrorCmdList->ResourceBarrier(barrierCount, toCopy);

      if (SUCCEEDED(mMirrorCmdList->Close())) {
        ID3D12CommandList* lists[] {mMirrorCmdList.get()};
        mCommandQueue->ExecuteCommandLists(1u, lists);
        writeSlot.mFenceValue = ++mMirrorFenceCounter;
        mCommandQueue->Signal(mMirrorFence.get(), writeSlot.mFenceValue);
        writeSlot.mSrcWidth = static_cast<uint32_t>(resDesc.Width);
        writeSlot.mSrcHeight = resDesc.Height;
        writeSlot.mRowPitch = footprint.Footprint.RowPitch;
        writeSlot.mFormat =
          (primaryFmt != DXGI_FORMAT_UNKNOWN) ? primaryFmt : resDesc.Format;
        writeSlot.mCropRect = primaryRect;
        writeSlot.mRightSameBuffer = rightSameBuffer;
        writeSlot.mRightCropRect = request.mRightRect;
        if (isBoth && !rightSameBuffer) {
          writeSlot.mRightSrcWidth = static_cast<uint32_t>(rightDesc.Width);
          writeSlot.mRightSrcHeight = rightDesc.Height;
          writeSlot.mRightRowPitch = rightFootprint.Footprint.RowPitch;
          writeSlot.mRightFormat =
            (rightFmt != DXGI_FORMAT_UNKNOWN) ? rightFmt : rightDesc.Format;
        }
        writeSlot.mEyeMode = request.mEyeMode;
        writeSlot.mTargetWidth = request.mTargetWidth;
        writeSlot.mTargetHeight = request.mTargetHeight;
        writeSlot.mPending = true;
        mMirrorWriteIdx = readIdx;
      }
    }
  }

  return producedFrame;
}

SHM::Reader& OpenXRD3D12Kneeboard::GetSHM() { return *mSHM; }

}// namespace VDXROVR
