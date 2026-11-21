// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <Windows.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace VDXROVR {

constexpr uint32_t kVRMirrorSharedMagic = 0x4D525856u; // "VXRM"
constexpr uint32_t kVRMirrorSharedVersion = 2u;
constexpr uint32_t kVRMirrorMaxWidth = 1920u;
constexpr uint32_t kVRMirrorMaxHeight = 1080u;
constexpr uint32_t kVRMirrorBytesPerPixel = 4u; // BGRA8_UNORM
constexpr uint32_t kVRMirrorSlotCount = 2u;
constexpr uint32_t kVRMirrorSlotBytes =
  kVRMirrorMaxWidth * kVRMirrorMaxHeight * kVRMirrorBytesPerPixel;
constexpr uint32_t kVRMirrorPixelFormatBGRA8 = 1u;
constexpr uint32_t kVRMirrorUseAppSetting = 0xFFu;

constexpr wchar_t kVRMirrorSharedMemoryName[] =
  L"Local\\VDXROVR.VRMirror.v2";

enum class VRMirrorGraphicsApi : uint8_t {
  Unknown = 0,
  D3D11 = 1,
  D3D12 = 2,
  Vulkan = 3,
};

struct alignas(8) VRMirrorSharedHeader {
  uint32_t mMagic {kVRMirrorSharedMagic};
  uint32_t mVersion {kVRMirrorSharedVersion};
  uint64_t mFrameSequence {0};
  uint32_t mActiveSlot {0};
  uint32_t mGameProcessId {0};
  uint32_t mWidth {1920};
  uint32_t mHeight {1080};
  uint32_t mStrideBytes {1920u * 4u};
  uint32_t mPixelFormat {kVRMirrorPixelFormatBGRA8};
  uint8_t mEyeMode {0};
  uint8_t mSmoothingEnabled {1};
  uint8_t mTemporalStrength {50};
  uint8_t mCameraStabilization {50};
  uint8_t mShowOverlays {1};
  uint8_t mIpdMm {63};
  uint8_t mCaptureGameAudio {1};
  uint8_t mWindowAudioMonitor {0};
  uint8_t mGraphicsApi {0};
  uint8_t mHasPixelData {0};
  uint16_t mOverlayLayerCount {0};
  uint32_t mSourceWidth {0};
  uint32_t mSourceHeight {0};
  uint32_t mConsumerDesiredEyeMode {kVRMirrorUseAppSetting};
  uint64_t mLeftEyeSwapchain {0};
  uint64_t mRightEyeSwapchain {0};
  float mStabilizedPosePos[3] {0.0f, 0.0f, 0.0f};
  float mStabilizedPoseQuat[4] {0.0f, 0.0f, 0.0f, 1.0f};
  float mRawAngularSpeedDegS {0.0f};
  float mStabilizedAngularSpeedDegS {0.0f};
  float mCurrentFrameWeight {1.0f};
  float mHistoryFrameWeight {0.0f};
  uint32_t mConsumerDesiredResolution {kVRMirrorUseAppSetting};
  uint64_t mProducerTimestampMs {0};
  uint64_t mWindowConsumerHeartbeatMs {0};
  uint64_t mObsConsumerHeartbeatMs {0};
  uint32_t mConsumerDesiredShowOverlays {kVRMirrorUseAppSetting};
  uint32_t mReserved[25] {};
};

static_assert(std::is_standard_layout_v<VRMirrorSharedHeader>);
static_assert(sizeof(VRMirrorSharedHeader) == 256);

struct VRMirrorSharedBuffer {
  VRMirrorSharedHeader mHeader {};
  uint8_t mSlots[kVRMirrorSlotCount][kVRMirrorSlotBytes] {};
};

static_assert(std::is_standard_layout_v<VRMirrorSharedBuffer>);

template <typename HandleT>
[[nodiscard]] inline uint64_t VRMirrorHandleToUInt64(
  const HandleT handle) noexcept {
  if constexpr (std::is_pointer_v<HandleT>) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(handle));
  } else if constexpr (std::is_integral_v<HandleT>) {
    return static_cast<uint64_t>(handle);
  } else if constexpr (requires { handle.mRawValue; }) {
    return static_cast<uint64_t>(handle.mRawValue);
  } else {
    static_assert(sizeof(HandleT) == sizeof(uint64_t));
    uint64_t raw = 0u;
    std::memcpy(&raw, &handle, sizeof(raw));
    return raw;
  }
}

[[nodiscard]] inline uint64_t VRMirrorReadU64Atomic(
  const uint64_t* field) noexcept {
  return static_cast<uint64_t>(InterlockedCompareExchange64(
    reinterpret_cast<volatile LONG64*>(const_cast<uint64_t*>(field)), 0, 0));
}

inline void VRMirrorWriteU64Atomic(
  uint64_t* field,
  const uint64_t value) noexcept {
  InterlockedExchange64(
    reinterpret_cast<volatile LONG64*>(field), static_cast<LONG64>(value));
}

[[nodiscard]] inline uint32_t VRMirrorReadU32Atomic(
  const uint32_t* field) noexcept {
  return static_cast<uint32_t>(InterlockedCompareExchange(
    reinterpret_cast<volatile LONG*>(const_cast<uint32_t*>(field)), 0, 0));
}

inline void VRMirrorWriteU32Atomic(
  uint32_t* field,
  const uint32_t value) noexcept {
  InterlockedExchange(
    reinterpret_cast<volatile LONG*>(field), static_cast<LONG>(value));
}

[[nodiscard]] inline bool IsVRMirrorConsumerActive(
  const VRMirrorSharedHeader& header,
  const uint64_t nowMs,
  const uint64_t timeoutMs = 1800u) noexcept {
  const uint64_t winHb = VRMirrorReadU64Atomic(&header.mWindowConsumerHeartbeatMs);
  const uint64_t obsHb = VRMirrorReadU64Atomic(&header.mObsConsumerHeartbeatMs);
  const bool winFresh = (winHb != 0u) && (nowMs >= winHb) && ((nowMs - winHb) <= timeoutMs);
  const bool obsFresh = (obsHb != 0u) && (nowMs >= obsHb) && ((nowMs - obsHb) <= timeoutMs);
  return winFresh || obsFresh;
}

[[nodiscard]] inline bool IsVRMirrorProducerActive(
  const VRMirrorSharedHeader& header,
  const uint64_t nowMs,
  const uint64_t timeoutMs = 2000u) noexcept {
  const uint64_t prodTs = VRMirrorReadU64Atomic(&header.mProducerTimestampMs);
  return (prodTs != 0u) && (nowMs >= prodTs) && ((nowMs - prodTs) <= timeoutMs);
}

}// namespace VDXROVR
