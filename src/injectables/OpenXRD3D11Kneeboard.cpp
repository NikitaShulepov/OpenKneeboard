// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.

#include "OpenXRD3D11Kneeboard.hpp"

#include "OpenXRNext.hpp"

#include <VDXROVR/D3D11.hpp>

#include <VDXROVR/config.hpp>
#include <VDXROVR/dprint.hpp>
#include <VDXROVR/scope_exit.hpp>
#include <VDXROVR/tracing.hpp>

#include <shims/winrt/base.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include <d3d11.h>

#include <directxtk/SpriteBatch.h>

#define XR_USE_GRAPHICS_API_D3D11
#include <openxr/openxr_platform.h>

using namespace DirectX::SimpleMath;

namespace VDXROVR {

OpenXRD3D11Kneeboard::OpenXRD3D11Kneeboard(
  XrInstance instance,
  XrSystemId systemID,
  XrSession session,
  OpenXRRuntimeID runtimeID,
  const std::shared_ptr<OpenXRNext>& next,
  const XrGraphicsBindingD3D11KHR& binding)
  : OpenXRKneeboard(instance, systemID, session, runtimeID, next) {
  dprint("{}", __FUNCTION__);
  VDXROVR_TraceLoggingScope("OpenXRD3D11Kneeboard()");

  mDevice.copy_from(binding.device);
  winrt::com_ptr<ID3D11DeviceContext> ctx;
  mDevice->GetImmediateContext(ctx.put());
  mImmediateContext = ctx.as<ID3D11DeviceContext1>();

  mRenderer = std::make_unique<D3D11::Renderer>(mDevice.get());

  mSHM = std::make_unique<SHM::D3D11::Reader>(
    SHM::ConsumerKind::OpenXR_D3D11, mDevice.get());
}

OpenXRD3D11Kneeboard::~OpenXRD3D11Kneeboard() {
  VDXROVR_TraceLoggingScope("~OpenXRD3D11Kneeboard()");
}

OpenXRD3D11Kneeboard::DXGIFormats OpenXRD3D11Kneeboard::GetDXGIFormats(
  OpenXRNext* oxr,
  XrSession session) {
  uint32_t formatCount {0};
  if (XR_FAILED(
        oxr->xrEnumerateSwapchainFormats(session, 0, &formatCount, nullptr))) {
    dprint("Failed to get swapchain format count");
    return {};
  }
  std::vector<int64_t> formats;
  formats.resize(formatCount);
  if (
    XR_FAILED(oxr->xrEnumerateSwapchainFormats(
      session, formatCount, &formatCount, formats.data()))
    || formatCount == 0) {
    dprint("Failed to enumerate swapchain formats");
    return {};
  }
  for (const auto it: formats) {
    dprint("Runtime supports swapchain format: {}", it);
  }
  // If this changes, we probably want to change the preference list below
  static_assert(SHM::SHARED_TEXTURE_PIXEL_FORMAT == DXGI_FORMAT_B8G8R8A8_UNORM);
  std::vector<DXGIFormats> preferredFormats {
    {DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, DXGI_FORMAT_B8G8R8A8_UNORM},
    {DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_R8G8B8A8_UNORM},
  };
  for (const auto preferred: preferredFormats) {
    auto it = std::ranges::find(formats, preferred.mTextureFormat);
    if (it != formats.end()) {
      return preferred;
    }
  }

  auto format = static_cast<DXGI_FORMAT>(formats.front());
  return {format, format};
}

XrSwapchain OpenXRD3D11Kneeboard::CreateSwapchain(
  XrSession session,
  const PixelSize& size) {
  dprint("{}", __FUNCTION__);
  VDXROVR_TraceLoggingScope("OpenXRD3D11Kneeboard::CreateSwapchain()");

  auto oxr = this->GetOpenXR();

  auto formats = GetDXGIFormats(oxr, session);
  dprint(
    "Creating swapchain with format {}",
    static_cast<int>(formats.mTextureFormat));

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
  if (XR_FAILED(nextResult)) {
    dprint("Failed to create swapchain: {}", nextResult);
    return nullptr;
  }

  uint32_t imageCount = 0;
  nextResult =
    oxr->xrEnumerateSwapchainImages(swapchain, 0, &imageCount, nullptr);
  if (imageCount == 0 || XR_FAILED(nextResult)) {
    dprint("No images in swapchain: {}", nextResult);
    return nullptr;
  }

  dprint("{} images in swapchain", imageCount);

  std::vector<XrSwapchainImageD3D11KHR> images;
  images.resize(
    imageCount,
    XrSwapchainImageD3D11KHR {
      .type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR,
    });
  nextResult = oxr->xrEnumerateSwapchainImages(
    swapchain,
    imageCount,
    &imageCount,
    reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data()));
  if (XR_FAILED(nextResult)) {
    dprint("Failed to enumerate images in swapchain: {}", nextResult);
    oxr->xrDestroySwapchain(swapchain);
    return nullptr;
  }

  if (images.at(0).type != XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR) {
    dprint("Swap chain is not a D3D11 swapchain");
    VDXROVR_BREAK;
    oxr->xrDestroySwapchain(swapchain);
    return nullptr;
  }

  std::vector<SwapchainBufferResources> buffers;
  buffers.reserve(imageCount);
  for (size_t i = 0; i < imageCount; ++i) {
    auto& image = images.at(i);
#ifdef DEBUG
    if (image.type != XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR) {
      VDXROVR_BREAK;
    }
#endif
    buffers.push_back(
      {mDevice.get(), image.texture, formats.mRenderTargetViewFormat});
  }

  mSwapchainResources[swapchain] = {
    .mDimensions = size,
    .mBufferResources = std::move(buffers),
  };
  {
    std::vector<winrt::com_ptr<ID3D11Texture2D>> overlayImgs;
    overlayImgs.reserve(imageCount);
    for (const auto& img : images) {
      winrt::com_ptr<ID3D11Texture2D> ptr;
      ptr.copy_from(img.texture);
      overlayImgs.push_back(std::move(ptr));
    }
    std::scoped_lock lock(mAppSwapchainsMutex);
    mOverlaySwapchainImages[VRMirrorHandleToUInt64(swapchain)] =
      std::move(overlayImgs);
  }

  return swapchain;
}

void OpenXRD3D11Kneeboard::ReleaseSwapchainResources(XrSwapchain swapchain) {
  if (mSwapchainResources.contains(swapchain)) {
    mSwapchainResources.erase(swapchain);
  }
  std::scoped_lock lock(mAppSwapchainsMutex);
  mOverlaySwapchainImages.erase(VRMirrorHandleToUInt64(swapchain));
}

void OpenXRD3D11Kneeboard::RenderLayers(
  XrSwapchain swapchain,
  uint32_t swapchainTextureIndex,
  SHM::Frame frame,
  const std::span<SHM::LayerSprite>& layers) {
  VDXROVR_TraceLoggingScope("OpenXRD3D11Kneeboard::RenderLayers()");

  D3D11::ScopedDeviceContextStateChange savedState(
    mImmediateContext, &mRenderState);

  mRenderer->RenderLayers(
    mSwapchainResources.at(swapchain),
    swapchainTextureIndex,
    mSHM->Map(std::move(frame)),
    layers,
    RenderMode::ClearAndRender);
}

namespace {

DXGI_FORMAT MakeMirrorSRVFormat(const DXGI_FORMAT fmt) noexcept {
  switch (fmt) {
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
      return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
      return DXGI_FORMAT_B8G8R8A8_UNORM;
    case DXGI_FORMAT_B8G8R8X8_TYPELESS:
      return DXGI_FORMAT_B8G8R8X8_UNORM;
    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
      return DXGI_FORMAT_R10G10B10A2_UNORM;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
      return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
      return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
      return DXGI_FORMAT_B8G8R8A8_UNORM;
    default:
      return fmt;
  }
}

RECT ComputeAspectCroppedSrcRect(
  const XrRect2Di& rect,
  const uint32_t texW,
  const uint32_t texH,
  const float targetAspect) noexcept {
  const int32_t rx = std::clamp(rect.offset.x, 0, static_cast<int32_t>(texW));
  const int32_t ry = std::clamp(rect.offset.y, 0, static_cast<int32_t>(texH));
  const int32_t rw = (rect.extent.width > 0)
    ? std::clamp(rect.extent.width, 1, static_cast<int32_t>(texW) - rx)
    : static_cast<int32_t>(texW) - rx;
  const int32_t rh = (rect.extent.height > 0)
    ? std::clamp(rect.extent.height, 1, static_cast<int32_t>(texH) - ry)
    : static_cast<int32_t>(texH) - ry;

  if (rw <= 0 || rh <= 0 || targetAspect <= 0.01f) {
    return RECT {0, 0, static_cast<LONG>(texW), static_cast<LONG>(texH)};
  }

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
  return RECT {
    static_cast<LONG>(cropX),
    static_cast<LONG>(cropY),
    static_cast<LONG>(cropX + cropW),
    static_cast<LONG>(cropY + cropH),
  };
}

}// namespace

void OpenXRD3D11Kneeboard::OnAppSwapchainCreated(
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
  std::vector<XrSwapchainImageD3D11KHR> images(
    imageCount,
    XrSwapchainImageD3D11KHR {.type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
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
  tracked.mFormat = static_cast<DXGI_FORMAT>(createInfo.format);
  tracked.mImages.reserve(imageCount);
  for (const auto& img : images) {
    winrt::com_ptr<ID3D11Texture2D> ptr;
    ptr.copy_from(img.texture);
    tracked.mImages.push_back(std::move(ptr));
  }

  std::scoped_lock lock(mAppSwapchainsMutex);
  mAppSwapchains[VRMirrorHandleToUInt64(swapchain)] = std::move(tracked);
}

void OpenXRD3D11Kneeboard::OnAppSwapchainDestroyed(const XrSwapchain swapchain) {
  std::scoped_lock lock(mAppSwapchainsMutex);
  mAppSwapchains.erase(VRMirrorHandleToUInt64(swapchain));
}

void OpenXRD3D11Kneeboard::OnAppSwapchainImageAcquired(
  const XrSwapchain swapchain,
  const uint32_t imageIndex) {
  std::scoped_lock lock(mAppSwapchainsMutex);
  if (auto it = mAppSwapchains.find(VRMirrorHandleToUInt64(swapchain));
      it != mAppSwapchains.end()) {
    it->second.mAcquiredQueue.push_back(imageIndex);
  }
}

void OpenXRD3D11Kneeboard::OnAppSwapchainImageReleased(
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

bool OpenXRD3D11Kneeboard::EnsureMirrorResources(
  const uint32_t width,
  const uint32_t height) noexcept {
  if (
    mMirrorRtTexture && mMirrorRtView && mMirrorStaging[0] && mMirrorStaging[1]
    && mMirrorRtWidth == width && mMirrorRtHeight == height) {
    return true;
  }

  mMirrorRtTexture = nullptr;
  mMirrorRtView = nullptr;
  mMirrorStaging[0] = nullptr;
  mMirrorStaging[1] = nullptr;
  mMirrorStagingReady = {false, false};
  mMirrorStagingWriteIdx = 0u;
  mMirrorRtWidth = width;
  mMirrorRtHeight = height;

  D3D11_TEXTURE2D_DESC rtDesc {};
  rtDesc.Width = width;
  rtDesc.Height = height;
  rtDesc.MipLevels = 1u;
  rtDesc.ArraySize = 1u;
  rtDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  rtDesc.SampleDesc = {1u, 0u};
  rtDesc.Usage = D3D11_USAGE_DEFAULT;
  rtDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

  if (FAILED(mDevice->CreateTexture2D(&rtDesc, nullptr, mMirrorRtTexture.put()))) {
    return false;
  }
  if (FAILED(mDevice->CreateRenderTargetView(
        mMirrorRtTexture.get(), nullptr, mMirrorRtView.put()))) {
    return false;
  }

  D3D11_TEXTURE2D_DESC stgDesc = rtDesc;
  stgDesc.Usage = D3D11_USAGE_STAGING;
  stgDesc.BindFlags = 0u;
  stgDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  for (auto& stg : mMirrorStaging) {
    if (FAILED(mDevice->CreateTexture2D(&stgDesc, nullptr, stg.put()))) {
      return false;
    }
  }

  if (!mMirrorSpriteBatch) {
    mMirrorSpriteBatch = std::make_unique<DirectX::SpriteBatch>(
      mImmediateContext.get());
  }
  if (!mMirrorStates) {
    mMirrorStates = std::make_unique<DirectX::CommonStates>(mDevice.get());
  }
  return true;
}

ID3D11ShaderResourceView* OpenXRD3D11Kneeboard::PrepareSourceSRV(
  ID3D11Texture2D* srcTex,
  const uint32_t arrayIndex,
  const DXGI_FORMAT declaredFormat) noexcept {
  if (!srcTex) {
    return nullptr;
  }
  D3D11_TEXTURE2D_DESC srcDesc {};
  srcTex->GetDesc(&srcDesc);
  const DXGI_FORMAT effectiveFmt =
    (declaredFormat != DXGI_FORMAT_UNKNOWN) ? declaredFormat : srcDesc.Format;
  const DXGI_FORMAT srvFmt = MakeMirrorSRVFormat(effectiveFmt);

  if (
    !mMirrorScratch2D || mMirrorScratchW != srcDesc.Width
    || mMirrorScratchH != srcDesc.Height || mMirrorScratchFmt != srvFmt) {
    mMirrorScratchSRV = nullptr;
    mMirrorScratch2D = nullptr;
    D3D11_TEXTURE2D_DESC scratchDesc {};
    scratchDesc.Width = srcDesc.Width;
    scratchDesc.Height = srcDesc.Height;
    scratchDesc.MipLevels = 1u;
    scratchDesc.ArraySize = 1u;
    scratchDesc.Format = srvFmt;
    scratchDesc.SampleDesc = {1u, 0u};
    scratchDesc.Usage = D3D11_USAGE_DEFAULT;
    scratchDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(mDevice->CreateTexture2D(
          &scratchDesc, nullptr, mMirrorScratch2D.put()))) {
      return nullptr;
    }
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc {};
    srvDesc.Format = srvFmt;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0u;
    srvDesc.Texture2D.MipLevels = 1u;
    if (FAILED(mDevice->CreateShaderResourceView(
          mMirrorScratch2D.get(), &srvDesc, mMirrorScratchSRV.put()))) {
      mMirrorScratch2D = nullptr;
      return nullptr;
    }
    mMirrorScratchW = srcDesc.Width;
    mMirrorScratchH = srcDesc.Height;
    mMirrorScratchFmt = srvFmt;
  }

  const uint32_t clampedSlice =
    (std::min)(arrayIndex, srcDesc.ArraySize > 0u ? (srcDesc.ArraySize - 1u) : 0u);
  const uint32_t srcSubresource =
    D3D11CalcSubresource(0u, clampedSlice, srcDesc.MipLevels);

  if (srcDesc.SampleDesc.Count > 1u) {
    mImmediateContext->ResolveSubresource(
      mMirrorScratch2D.get(), 0u, srcTex, srcSubresource, srvFmt);
  } else {
    mImmediateContext->CopySubresourceRegion(
      mMirrorScratch2D.get(), 0u, 0u, 0u, 0u, srcTex, srcSubresource, nullptr);
  }
  return mMirrorScratchSRV.get();
}

bool OpenXRD3D11Kneeboard::CaptureMirrorFrame(
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

  winrt::com_ptr<ID3D11Texture2D> leftTex;
  winrt::com_ptr<ID3D11Texture2D> rightTex;
  winrt::com_ptr<ID3D11Texture2D> overlayTex;
  DXGI_FORMAT leftFmt = DXGI_FORMAT_UNKNOWN;
  DXGI_FORMAT rightFmt = DXGI_FORMAT_UNKNOWN;
  uint32_t leftW = 0u;
  uint32_t leftH = 0u;
  uint32_t rightW = 0u;
  uint32_t rightH = 0u;

  {
    std::scoped_lock lock(mAppSwapchainsMutex);
    const auto leftIt =
      mAppSwapchains.find(VRMirrorHandleToUInt64(request.mLeftSwapchain));
    if (
      leftIt != mAppSwapchains.end() && !leftIt->second.mImages.empty()) {
      const uint32_t idx = leftIt->second.mHasReleased
        ? leftIt->second.mLastReleasedIndex
        : 0u;
      if (idx < leftIt->second.mImages.size()) {
        leftTex = leftIt->second.mImages[idx];
        leftFmt = leftIt->second.mFormat;
        leftW = leftIt->second.mWidth;
        leftH = leftIt->second.mHeight;
      }
    }
    const auto rightIt =
      mAppSwapchains.find(VRMirrorHandleToUInt64(request.mRightSwapchain));
    if (
      rightIt != mAppSwapchains.end() && !rightIt->second.mImages.empty()) {
      const uint32_t idx = rightIt->second.mHasReleased
        ? rightIt->second.mLastReleasedIndex
        : 0u;
      if (idx < rightIt->second.mImages.size()) {
        rightTex = rightIt->second.mImages[idx];
        rightFmt = rightIt->second.mFormat;
        rightW = rightIt->second.mWidth;
        rightH = rightIt->second.mHeight;
      }
    }
    if (request.mHasOverlay && request.mShowOverlays) {
      const auto ovlIt =
        mOverlaySwapchainImages.find(VRMirrorHandleToUInt64(request.mOverlaySwapchain));
      if (
        ovlIt != mOverlaySwapchainImages.end()
        && request.mOverlayImageIndex < ovlIt->second.size()) {
        overlayTex = ovlIt->second[request.mOverlayImageIndex];
      }
    }
  }

  if (!leftTex && !rightTex) {
    return false;
  }
  if (!leftTex) {
    leftTex = rightTex;
    leftFmt = rightFmt;
    leftW = rightW;
    leftH = rightH;
  }
  if (!rightTex) {
    rightTex = leftTex;
    rightFmt = leftFmt;
    rightW = leftW;
    rightH = leftH;
  }

  if (!this->EnsureMirrorResources(request.mTargetWidth, request.mTargetHeight)) {
    return false;
  }

  outSourceWidth = leftW;
  outSourceHeight = leftH;

  {
    D3D11::ScopedDeviceContextStateChange savedState(
      mImmediateContext, &mRenderState);

    ID3D11RenderTargetView* rtv = mMirrorRtView.get();
    mImmediateContext->OMSetRenderTargets(1u, &rtv, nullptr);
    const float clearColor[4] {0.039f, 0.051f, 0.078f, 1.0f};
    mImmediateContext->ClearRenderTargetView(rtv, clearColor);

    const D3D11_VIEWPORT vp {
      .TopLeftX = 0.0f,
      .TopLeftY = 0.0f,
      .Width = static_cast<float>(request.mTargetWidth),
      .Height = static_cast<float>(request.mTargetHeight),
      .MinDepth = 0.0f,
      .MaxDepth = 1.0f,
    };
    mImmediateContext->RSSetViewports(1u, &vp);
    mMirrorSpriteBatch->SetViewport(vp);

    const float fullAspect = static_cast<float>(request.mTargetWidth)
      / static_cast<float>(request.mTargetHeight);

    if (request.mEyeMode == VRMirrorEyeMode::Both) {
      const uint32_t halfW = request.mTargetWidth / 2u;
      const float halfAspect =
        static_cast<float>(halfW) / static_cast<float>(request.mTargetHeight);

      if (auto* leftSrv = this->PrepareSourceSRV(
            leftTex.get(), request.mLeftArrayIndex, leftFmt)) {
        const RECT srcL = ComputeAspectCroppedSrcRect(
          request.mLeftRect, leftW, leftH, halfAspect);
        const RECT dstL {0, 0, static_cast<LONG>(halfW), static_cast<LONG>(request.mTargetHeight)};
        mMirrorSpriteBatch->Begin(
          DirectX::SpriteSortMode_Immediate,
          mMirrorStates->Opaque(),
          mMirrorStates->LinearClamp());
        mMirrorSpriteBatch->Draw(leftSrv, dstL, &srcL);
        mMirrorSpriteBatch->End();
      }

      if (auto* rightSrv = this->PrepareSourceSRV(
            rightTex.get(), request.mRightArrayIndex, rightFmt)) {
        const RECT srcR = ComputeAspectCroppedSrcRect(
          request.mRightRect, rightW, rightH, halfAspect);
        const RECT dstR {
          static_cast<LONG>(halfW),
          0,
          static_cast<LONG>(request.mTargetWidth),
          static_cast<LONG>(request.mTargetHeight)};
        mMirrorSpriteBatch->Begin(
          DirectX::SpriteSortMode_Immediate,
          mMirrorStates->Opaque(),
          mMirrorStates->LinearClamp());
        mMirrorSpriteBatch->Draw(rightSrv, dstR, &srcR);
        mMirrorSpriteBatch->End();
      }
    } else {
      const bool useRight = (request.mEyeMode == VRMirrorEyeMode::Right);
      ID3D11Texture2D* eyeTex = useRight ? rightTex.get() : leftTex.get();
      const uint32_t eyeSlice =
        useRight ? request.mRightArrayIndex : request.mLeftArrayIndex;
      const DXGI_FORMAT eyeFmt = useRight ? rightFmt : leftFmt;
      const XrRect2Di eyeRect = useRight ? request.mRightRect : request.mLeftRect;
      const uint32_t eyeW = useRight ? rightW : leftW;
      const uint32_t eyeH = useRight ? rightH : leftH;

      if (auto* eyeSrv = this->PrepareSourceSRV(eyeTex, eyeSlice, eyeFmt)) {
        const RECT src = ComputeAspectCroppedSrcRect(eyeRect, eyeW, eyeH, fullAspect);
        const RECT dst {
          0,
          0,
          static_cast<LONG>(request.mTargetWidth),
          static_cast<LONG>(request.mTargetHeight)};
        mMirrorSpriteBatch->Begin(
          DirectX::SpriteSortMode_Immediate,
          mMirrorStates->Opaque(),
          mMirrorStates->LinearClamp());
        mMirrorSpriteBatch->Draw(eyeSrv, dst, &src);
        mMirrorSpriteBatch->End();
      }
    }

    if (overlayTex && request.mShowOverlays) {
      if (auto* ovlSrv = this->PrepareSourceSRV(
            overlayTex.get(), 0u, DXGI_FORMAT_B8G8R8A8_UNORM)) {
        const LONG ovlW = static_cast<LONG>(request.mTargetWidth / 3u);
        const LONG ovlH = static_cast<LONG>(request.mTargetHeight / 2u);
        const RECT dstOvl {
          static_cast<LONG>(request.mTargetWidth) - ovlW - 24,
          static_cast<LONG>(request.mTargetHeight) - ovlH - 24,
          static_cast<LONG>(request.mTargetWidth) - 24,
          static_cast<LONG>(request.mTargetHeight) - 24};
        mMirrorSpriteBatch->Begin(
          DirectX::SpriteSortMode_Immediate,
          mMirrorStates->AlphaBlend(),
          mMirrorStates->LinearClamp());
        mMirrorSpriteBatch->Draw(ovlSrv, dstOvl);
        mMirrorSpriteBatch->End();
      }
    }

    const uint32_t writeIdx = mMirrorStagingWriteIdx & 1u;
    const uint32_t readIdx = writeIdx ^ 1u;
    mImmediateContext->CopyResource(
      mMirrorStaging[writeIdx].get(), mMirrorRtTexture.get());
    mMirrorStagingReady[writeIdx] = true;
    mMirrorStagingWriteIdx = readIdx;

    if (!mMirrorStagingReady[readIdx]) {
      return false;
    }

    D3D11_MAPPED_SUBRESOURCE mapped {};
    const HRESULT mapHr = mImmediateContext->Map(
      mMirrorStaging[readIdx].get(),
      0u,
      D3D11_MAP_READ,
      D3D11_MAP_FLAG_DO_NOT_WAIT,
      &mapped);
    if (FAILED(mapHr) || !mapped.pData) {
      return false;
    }

    const size_t rowBytes = static_cast<size_t>(request.mTargetWidth) * 4u;
    outBgra.resize(rowBytes * static_cast<size_t>(request.mTargetHeight));
    const auto* srcBytes = static_cast<const uint8_t*>(mapped.pData);
    for (uint32_t y = 0; y < request.mTargetHeight; ++y) {
      std::memcpy(
        outBgra.data() + (static_cast<size_t>(y) * rowBytes),
        srcBytes + (static_cast<size_t>(y) * mapped.RowPitch),
        rowBytes);
    }
    mImmediateContext->Unmap(mMirrorStaging[readIdx].get(), 0u);
    outWidth = request.mTargetWidth;
    outHeight = request.mTargetHeight;
    return true;
  }
}

SHM::Reader& OpenXRD3D11Kneeboard::GetSHM() { return *mSHM; }

}// namespace VDXROVR
