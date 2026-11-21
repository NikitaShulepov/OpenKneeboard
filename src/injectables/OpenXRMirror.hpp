// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <VDXROVR/VRKneeboard.hpp>
#include <VDXROVR/VRMirrorSHM.hpp>
#include <VDXROVR/VRSettings.hpp>

#include <shims/openxr/openxr.h>

#include <Windows.h>

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace VDXROVR {

struct VRMirrorTemporalWeights {
  float mCurrentWeight {1.0f};
  float mHistoryWeight {0.0f};
};

class OpenXRMirror final {
 public:
  static constexpr size_t kTemporalRingCapacity = 3;

  struct CaptureRequest {
    XrSwapchain mLeftSwapchain {XR_NULL_HANDLE};
    XrRect2Di mLeftRect {};
    uint32_t mLeftArrayIndex {0};
    XrSwapchain mRightSwapchain {XR_NULL_HANDLE};
    XrRect2Di mRightRect {};
    uint32_t mRightArrayIndex {0};
    bool mHasRightEye {false};
    XrSwapchain mOverlaySwapchain {XR_NULL_HANDLE};
    uint32_t mOverlayImageIndex {0};
    bool mHasOverlay {false};
    VRMirrorEyeMode mEyeMode {VRMirrorEyeMode::Left};
    uint32_t mTargetWidth {1920};
    uint32_t mTargetHeight {1080};
    bool mShowOverlays {true};
  };

  OpenXRMirror() = default;
  ~OpenXRMirror();

  OpenXRMirror(const OpenXRMirror&) = delete;
  OpenXRMirror& operator=(const OpenXRMirror&) = delete;

  [[nodiscard]] static VRMirrorTemporalWeights ComputeTemporalWeights(
    size_t availableFrameCount,
    uint8_t temporalStrength,
    bool smoothingEnabled) noexcept;

  [[nodiscard]] bool HasActiveConsumer() noexcept;

  [[nodiscard]] VRMirrorSettings ResolveEffectiveSettings(
    const VRMirrorSettings& appSettings) noexcept;

  [[nodiscard]] bool BuildCaptureRequest(
    const XrFrameEndInfo* originalFrameEndInfo,
    const VRMirrorSettings& effectiveSettings,
    XrSwapchain overlaySwapchain,
    uint32_t overlayImageIndex,
    bool hasOverlay,
    CaptureRequest& outRequest) const noexcept;

  // Blends a newly captured BGRA8 frame into the 3-frame temporal ring
  // using exact VDXR_Mirror temporal blending weights.
  void PushAndBlendTemporalFrame(
    std::span<const uint8_t> currentBgra,
    uint32_t width,
    uint32_t height,
    uint8_t temporalStrength,
    bool smoothingEnabled,
    std::vector<uint8_t>& outBgra);

  void PublishEndFrame(
    const XrFrameEndInfo* originalFrameEndInfo,
    std::span<const XrCompositionLayerBaseHeader* const> composedLayers,
    const VRMirrorSettings& effectiveSettings,
    const VRKneeboard::Pose& rawHmdPose,
    float dtSeconds,
    VRMirrorGraphicsApi graphicsApi,
    std::span<const uint8_t> capturedBgra,
    uint32_t capturedWidth,
    uint32_t capturedHeight,
    uint32_t sourceWidth,
    uint32_t sourceHeight) noexcept;

 private:
  void EnsureSharedMemory() noexcept;
  void StepStabilizer(
    const VRKneeboard::Pose& rawHmdPose,
    float dtSeconds,
    uint8_t stabilizationStrength,
    bool enabled) noexcept;

  HANDLE mMappingHandle {nullptr};
  VRMirrorSharedBuffer* mSharedBuffer {nullptr};
  uint64_t mFrameSequence {0};
  bool mStabilizerInitialized {false};
  VRKneeboard::Pose mStabilizedPose {};
  VRKneeboard::Pose mLastRawPose {};
  float mRawAngularSpeedDegS {0.0f};
  float mStabilizedAngularSpeedDegS {0.0f};

  std::array<std::vector<uint8_t>, kTemporalRingCapacity> mFrameRing {};
  size_t mRingHead {0};
  size_t mRingCount {0};
  uint32_t mLastFrameWidth {0};
  uint32_t mLastFrameHeight {0};
  std::vector<uint8_t> mBlendedScratch {};
};

}// namespace VDXROVR
