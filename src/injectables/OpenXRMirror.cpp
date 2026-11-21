// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include "OpenXRMirror.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>

using namespace DirectX::SimpleMath;

namespace VDXROVR {

namespace {

float QuatAngleDeg(const Quaternion& a, const Quaternion& b) noexcept {
  const float dot = std::clamp(std::abs(a.Dot(b)), 0.0f, 1.0f);
  return DirectX::XMConvertToDegrees(2.0f * std::acos(dot));
}

}// namespace

OpenXRMirror::~OpenXRMirror() {
  if (mSharedBuffer) {
    VRMirrorWriteU64Atomic(&mSharedBuffer->mHeader.mProducerTimestampMs, 0u);
    VRMirrorWriteU32Atomic(&mSharedBuffer->mHeader.mGameProcessId, 0u);
    mSharedBuffer->mHeader.mHasPixelData = 0u;
    UnmapViewOfFile(mSharedBuffer);
    mSharedBuffer = nullptr;
  }
  if (mMappingHandle) {
    CloseHandle(mMappingHandle);
    mMappingHandle = nullptr;
  }
}

void OpenXRMirror::EnsureSharedMemory() noexcept {
  if (mSharedBuffer) {
    return;
  }
  const uint64_t totalBytes = sizeof(VRMirrorSharedBuffer);
  const DWORD sizeHigh = static_cast<DWORD>(totalBytes >> 32);
  const DWORD sizeLow = static_cast<DWORD>(totalBytes & 0xFFFFFFFFull);
  mMappingHandle = CreateFileMappingW(
    INVALID_HANDLE_VALUE,
    nullptr,
    PAGE_READWRITE,
    sizeHigh,
    sizeLow,
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
    mSharedBuffer->mHeader.mStabilizedPoseQuat[3] = 1.0f;
  }
}

bool OpenXRMirror::HasActiveConsumer() noexcept {
  this->EnsureSharedMemory();
  if (!mSharedBuffer) {
    return false;
  }
  return IsVRMirrorConsumerActive(mSharedBuffer->mHeader, GetTickCount64());
}

VRMirrorSettings OpenXRMirror::ResolveEffectiveSettings(
  const VRMirrorSettings& appSettings) noexcept {
  VRMirrorSettings out = appSettings;
  this->EnsureSharedMemory();
  if (!mSharedBuffer) {
    return out;
  }
  const auto& hdr = mSharedBuffer->mHeader;
  const uint32_t eyeOv = VRMirrorReadU32Atomic(&hdr.mConsumerDesiredEyeMode);
  if (eyeOv <= static_cast<uint32_t>(VRMirrorEyeMode::Both)) {
    out.mEyeMode = static_cast<VRMirrorEyeMode>(eyeOv);
  }
  const uint32_t resOv = VRMirrorReadU32Atomic(&hdr.mConsumerDesiredResolution);
  if (resOv <= static_cast<uint32_t>(VRMirrorResolution::FHD1080)) {
    out.mResolution = static_cast<VRMirrorResolution>(resOv);
  }
  const uint32_t ovlOv = VRMirrorReadU32Atomic(&hdr.mConsumerDesiredShowOverlays);
  if (ovlOv <= 1u) {
    out.mShowOverlays = (ovlOv != 0u);
  }
  return out;
}

bool OpenXRMirror::BuildCaptureRequest(
  const XrFrameEndInfo* originalFrameEndInfo,
  const VRMirrorSettings& effectiveSettings,
  const XrSwapchain overlaySwapchain,
  const uint32_t overlayImageIndex,
  const bool hasOverlay,
  CaptureRequest& outRequest) const noexcept {
  if (!originalFrameEndInfo || !originalFrameEndInfo->layers) {
    return false;
  }

  for (uint32_t i = 0; i < originalFrameEndInfo->layerCount; ++i) {
    const auto* layer = originalFrameEndInfo->layers[i];
    if (!layer || layer->type != XR_TYPE_COMPOSITION_LAYER_PROJECTION) {
      continue;
    }
    const auto* proj =
      reinterpret_cast<const XrCompositionLayerProjection*>(layer);
    if (proj->viewCount < 1u || !proj->views) {
      continue;
    }
    outRequest = {};
    outRequest.mLeftSwapchain = proj->views[0].subImage.swapchain;
    outRequest.mLeftRect = proj->views[0].subImage.imageRect;
    outRequest.mLeftArrayIndex = proj->views[0].subImage.imageArrayIndex;
    if (proj->viewCount >= 2u) {
      outRequest.mRightSwapchain = proj->views[1].subImage.swapchain;
      outRequest.mRightRect = proj->views[1].subImage.imageRect;
      outRequest.mRightArrayIndex = proj->views[1].subImage.imageArrayIndex;
      outRequest.mHasRightEye = true;
    } else {
      outRequest.mRightSwapchain = outRequest.mLeftSwapchain;
      outRequest.mRightRect = outRequest.mLeftRect;
      outRequest.mRightArrayIndex = outRequest.mLeftArrayIndex;
      outRequest.mHasRightEye = false;
    }
    outRequest.mOverlaySwapchain = overlaySwapchain;
    outRequest.mOverlayImageIndex = overlayImageIndex;
    outRequest.mHasOverlay = hasOverlay && effectiveSettings.mShowOverlays;
    outRequest.mEyeMode = effectiveSettings.mEyeMode;
    outRequest.mTargetWidth =
      (effectiveSettings.mResolution == VRMirrorResolution::HD720) ? 1280u : 1920u;
    outRequest.mTargetHeight =
      (effectiveSettings.mResolution == VRMirrorResolution::HD720) ? 720u : 1080u;
    outRequest.mShowOverlays = effectiveSettings.mShowOverlays;
    return static_cast<bool>(outRequest.mLeftSwapchain);
  }
  return false;
}

VRMirrorTemporalWeights OpenXRMirror::ComputeTemporalWeights(
  const size_t availableFrameCount,
  const uint8_t temporalStrength,
  const bool smoothingEnabled) noexcept {
  if (!smoothingEnabled || temporalStrength == 0u || availableFrameCount <= 1u) {
    return {.mCurrentWeight = 1.0f, .mHistoryWeight = 0.0f};
  }
  const float alpha =
    std::clamp(static_cast<float>(temporalStrength) / 100.0f, 0.0f, 0.75f);
  const float currentWeight = 1.0f - alpha;
  const float historyWeight =
    alpha / static_cast<float>(availableFrameCount - 1u);
  return {
    .mCurrentWeight = currentWeight,
    .mHistoryWeight = historyWeight,
  };
}

void OpenXRMirror::PushAndBlendTemporalFrame(
  const std::span<const uint8_t> currentBgra,
  const uint32_t width,
  const uint32_t height,
  const uint8_t temporalStrength,
  const bool smoothingEnabled,
  std::vector<uint8_t>& outBgra) {
  const size_t byteCount =
    static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
  if (byteCount == 0u || currentBgra.size() < byteCount) {
    outBgra.clear();
    return;
  }

  if (width != mLastFrameWidth || height != mLastFrameHeight) {
    mRingHead = 0u;
    mRingCount = 0u;
    mLastFrameWidth = width;
    mLastFrameHeight = height;
    for (auto& slot : mFrameRing) {
      slot.resize(byteCount);
    }
  }

  auto& writeSlot = mFrameRing[mRingHead];
  if (writeSlot.size() != byteCount) {
    writeSlot.resize(byteCount);
  }
  std::memcpy(writeSlot.data(), currentBgra.data(), byteCount);

  const size_t currentIdx = mRingHead;
  mRingHead = (mRingHead + 1u) % kTemporalRingCapacity;
  mRingCount = (std::min)(mRingCount + 1u, kTemporalRingCapacity);

  const auto weights =
    ComputeTemporalWeights(mRingCount, temporalStrength, smoothingEnabled);

  outBgra.resize(byteCount);
  if (weights.mHistoryWeight <= 1e-5f || mRingCount <= 1u) {
    std::memcpy(outBgra.data(), currentBgra.data(), byteCount);
    return;
  }

  const uint32_t wCur = static_cast<uint32_t>(std::lround(weights.mCurrentWeight * 256.0f));
  const uint32_t wHist = static_cast<uint32_t>(std::lround(weights.mHistoryWeight * 256.0f));
  const uint8_t* curPtr = mFrameRing[currentIdx].data();
  const size_t histCount = mRingCount - 1u;
  const uint8_t* hist0 =
    mFrameRing[(currentIdx + kTemporalRingCapacity - 1u) % kTemporalRingCapacity].data();
  const uint8_t* hist1 = (histCount >= 2u)
    ? mFrameRing[(currentIdx + kTemporalRingCapacity - 2u) % kTemporalRingCapacity].data()
    : nullptr;

  uint8_t* dst = outBgra.data();
  if (hist1) {
    for (size_t i = 0; i < byteCount; ++i) {
      const uint32_t acc =
        (curPtr[i] * wCur) + ((static_cast<uint32_t>(hist0[i]) + hist1[i]) * wHist) + 128u;
      dst[i] = static_cast<uint8_t>((std::min)(acc >> 8, 255u));
    }
  } else {
    for (size_t i = 0; i < byteCount; ++i) {
      const uint32_t acc = (curPtr[i] * wCur) + (hist0[i] * wHist) + 128u;
      dst[i] = static_cast<uint8_t>((std::min)(acc >> 8, 255u));
    }
  }
}

void OpenXRMirror::StepStabilizer(
  const VRKneeboard::Pose& rawHmdPose,
  const float dtSeconds,
  const uint8_t stabilizationStrength,
  const bool enabled) noexcept {
  const float dt = std::clamp(dtSeconds, 0.001f, 0.1f);
  if (!mStabilizerInitialized) {
    mStabilizedPose = rawHmdPose;
    mLastRawPose = rawHmdPose;
    mRawAngularSpeedDegS = 0.0f;
    mStabilizedAngularSpeedDegS = 0.0f;
    mStabilizerInitialized = true;
    return;
  }

  mRawAngularSpeedDegS =
    QuatAngleDeg(mLastRawPose.mOrientation, rawHmdPose.mOrientation) / dt;

  const float strength =
    enabled
    ? std::clamp(static_cast<float>(stabilizationStrength) / 100.0f, 0.0f, 0.75f)
    : 0.0f;
  if (strength <= 1e-4f) {
    mStabilizedPose = rawHmdPose;
    mStabilizedAngularSpeedDegS = mRawAngularSpeedDegS;
    mLastRawPose = rawHmdPose;
    return;
  }

  const float lambda = 26.0f * (1.0f - (strength * 0.90f));
  const float t = 1.0f - std::exp(-lambda * dt);

  VRKneeboard::Pose nextPose {};
  nextPose.mPosition = Vector3::Lerp(mStabilizedPose.mPosition, rawHmdPose.mPosition, t);
  nextPose.mOrientation =
    Quaternion::Slerp(mStabilizedPose.mOrientation, rawHmdPose.mOrientation, t);
  nextPose.mOrientation.Normalize();

  mStabilizedAngularSpeedDegS =
    QuatAngleDeg(mStabilizedPose.mOrientation, nextPose.mOrientation) / dt;
  mStabilizedPose = nextPose;
  mLastRawPose = rawHmdPose;
}

void OpenXRMirror::PublishEndFrame(
  const XrFrameEndInfo* originalFrameEndInfo,
  const std::span<const XrCompositionLayerBaseHeader* const> composedLayers,
  const VRMirrorSettings& effectiveSettings,
  const VRKneeboard::Pose& rawHmdPose,
  const float dtSeconds,
  const VRMirrorGraphicsApi graphicsApi,
  const std::span<const uint8_t> capturedBgra,
  const uint32_t capturedWidth,
  const uint32_t capturedHeight,
  const uint32_t sourceWidth,
  const uint32_t sourceHeight) noexcept {
  if (!originalFrameEndInfo) {
    return;
  }

  this->EnsureSharedMemory();
  if (!mSharedBuffer) {
    return;
  }

  this->StepStabilizer(
    rawHmdPose,
    dtSeconds,
    effectiveSettings.mCameraStabilization,
    effectiveSettings.mSmoothingEnabled);

  uint64_t leftSwapchain = 0;
  uint64_t rightSwapchain = 0;
  for (uint32_t i = 0; i < originalFrameEndInfo->layerCount; ++i) {
    const auto* layer = originalFrameEndInfo->layers[i];
    if (!layer || layer->type != XR_TYPE_COMPOSITION_LAYER_PROJECTION) {
      continue;
    }
    const auto* proj =
      reinterpret_cast<const XrCompositionLayerProjection*>(layer);
    if (proj->viewCount >= 1 && proj->views) {
      leftSwapchain = VRMirrorHandleToUInt64(proj->views[0].subImage.swapchain);
    }
    if (proj->viewCount >= 2 && proj->views) {
      rightSwapchain = VRMirrorHandleToUInt64(proj->views[1].subImage.swapchain);
    }
    break;
  }

  uint16_t overlayCount = 0;
  if (
    effectiveSettings.mShowOverlays
    && composedLayers.size() > originalFrameEndInfo->layerCount) {
    overlayCount = static_cast<uint16_t>(
      composedLayers.size() - originalFrameEndInfo->layerCount);
  }

  auto& hdr = mSharedBuffer->mHeader;
  const uint32_t outW = (capturedWidth > 0u && capturedWidth <= kVRMirrorMaxWidth)
    ? capturedWidth
    : ((effectiveSettings.mResolution == VRMirrorResolution::HD720) ? 1280u : 1920u);
  const uint32_t outH = (capturedHeight > 0u && capturedHeight <= kVRMirrorMaxHeight)
    ? capturedHeight
    : ((effectiveSettings.mResolution == VRMirrorResolution::HD720) ? 720u : 1080u);
  const size_t expectedBytes = static_cast<size_t>(outW) * static_cast<size_t>(outH) * 4u;

  uint32_t activeSlot = VRMirrorReadU32Atomic(&hdr.mActiveSlot) & 1u;
  bool publishedPixels = false;

  if (!capturedBgra.empty() && capturedBgra.size() >= expectedBytes && expectedBytes <= kVRMirrorSlotBytes) {
    try {
      this->PushAndBlendTemporalFrame(
        capturedBgra.subspan(0, expectedBytes),
        outW,
        outH,
        effectiveSettings.mTemporalStrength,
        effectiveSettings.mSmoothingEnabled,
        mBlendedScratch);
      const uint32_t writeSlot = activeSlot ^ 1u;
      std::memcpy(
        mSharedBuffer->mSlots[writeSlot],
        mBlendedScratch.data(),
        expectedBytes);
      std::atomic_thread_fence(std::memory_order_release);
      activeSlot = writeSlot;
      publishedPixels = true;
    } catch (...) {
      publishedPixels = false;
    }
  }

  const auto weights = ComputeTemporalWeights(
    (std::max)(mRingCount, size_t {1u}),
    effectiveSettings.mTemporalStrength,
    effectiveSettings.mSmoothingEnabled);

  hdr.mMagic = kVRMirrorSharedMagic;
  hdr.mVersion = kVRMirrorSharedVersion;
  VRMirrorWriteU32Atomic(&hdr.mGameProcessId, GetCurrentProcessId());
  hdr.mWidth = outW;
  hdr.mHeight = outH;
  hdr.mStrideBytes = outW * 4u;
  hdr.mPixelFormat = kVRMirrorPixelFormatBGRA8;
  hdr.mEyeMode = static_cast<uint8_t>(effectiveSettings.mEyeMode);
  hdr.mSmoothingEnabled = effectiveSettings.mSmoothingEnabled ? 1u : 0u;
  hdr.mTemporalStrength = effectiveSettings.mTemporalStrength;
  hdr.mCameraStabilization = effectiveSettings.mCameraStabilization;
  hdr.mShowOverlays = effectiveSettings.mShowOverlays ? 1u : 0u;
  hdr.mIpdMm = effectiveSettings.mIpdMm;
  hdr.mCaptureGameAudio = effectiveSettings.mCaptureGameAudio ? 1u : 0u;
  hdr.mWindowAudioMonitor = effectiveSettings.mWindowAudioMonitor ? 1u : 0u;
  hdr.mGraphicsApi = static_cast<uint8_t>(graphicsApi);
  if (publishedPixels) {
    hdr.mHasPixelData = 1u;
  }
  hdr.mOverlayLayerCount = overlayCount;
  hdr.mSourceWidth = sourceWidth;
  hdr.mSourceHeight = sourceHeight;
  hdr.mLeftEyeSwapchain = leftSwapchain;
  hdr.mRightEyeSwapchain = rightSwapchain;
  hdr.mStabilizedPosePos[0] = mStabilizedPose.mPosition.x;
  hdr.mStabilizedPosePos[1] = mStabilizedPose.mPosition.y;
  hdr.mStabilizedPosePos[2] = mStabilizedPose.mPosition.z;
  hdr.mStabilizedPoseQuat[0] = mStabilizedPose.mOrientation.x;
  hdr.mStabilizedPoseQuat[1] = mStabilizedPose.mOrientation.y;
  hdr.mStabilizedPoseQuat[2] = mStabilizedPose.mOrientation.z;
  hdr.mStabilizedPoseQuat[3] = mStabilizedPose.mOrientation.w;
  hdr.mRawAngularSpeedDegS = mRawAngularSpeedDegS;
  hdr.mStabilizedAngularSpeedDegS = mStabilizedAngularSpeedDegS;
  hdr.mCurrentFrameWeight = weights.mCurrentWeight;
  hdr.mHistoryFrameWeight = weights.mHistoryWeight;
  VRMirrorWriteU64Atomic(&hdr.mProducerTimestampMs, GetTickCount64());

  std::atomic_thread_fence(std::memory_order_release);
  if (publishedPixels) {
    VRMirrorWriteU32Atomic(&hdr.mActiveSlot, activeSlot);
    VRMirrorWriteU64Atomic(&hdr.mFrameSequence, ++mFrameSequence);
  }
}

}// namespace VDXROVR
