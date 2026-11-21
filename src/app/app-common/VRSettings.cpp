// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <VDXROVR/VRSettings.hpp>

#include <VDXROVR/json/VRSettings.hpp>

#include <VDXROVR/json.hpp>

namespace VDXROVR {

VRPose VRPose::GetHorizontalMirror() const {
  auto ret = *this;
  ret.mX = -ret.mX;
  // Yaw
  ret.mRY = -ret.mRY;
  // Roll
  ret.mRZ = -ret.mRZ;
  return ret;
}

VDXROVR_DEFINE_SPARSE_JSON(VRPose, mX, mEyeY, mZ, mRX, mRY, mRZ)

NLOHMANN_JSON_SERIALIZE_ENUM(
  VRRenderSettings::Quirks::Upscaling,
  {
    {VRRenderSettings::Quirks::Upscaling::Automatic, "Automatic"},
    {VRRenderSettings::Quirks::Upscaling::AlwaysOff, "AlwaysOff"},
    {VRRenderSettings::Quirks::Upscaling::AlwaysOn, "AlwaysOn"},
  });

VDXROVR_DEFINE_SPARSE_JSON(VRRenderSettings::Quirks, mOpenXR_Upscaling)

VDXROVR_DEFINE_SPARSE_JSON(GazeTargetScale, mVertical, mHorizontal);

VDXROVR_DEFINE_SPARSE_JSON(VROpacitySettings, mNormal, mGaze);

NLOHMANN_JSON_SERIALIZE_ENUM(
  VRFollowMode,
  {
    {VRFollowMode::WorldLocked, "WorldLocked"},
    {VRFollowMode::HeadLocked, "HeadLocked"},
    {VRFollowMode::HeadFollowSmooth, "HeadFollowSmooth"},
    {VRFollowMode::HeadFollowLazy, "HeadFollowLazy"},
    {VRFollowMode::LeftHand, "LeftHand"},
    {VRFollowMode::RightHand, "RightHand"},
  });
VDXROVR_DEFINE_SPARSE_JSON(
  VRHeadFollowSettings,
  mMode,
  mDeadzoneAngleDeg,
  mDeadzoneDistanceM,
  mFollowSpeed,
  mLockRoll,
  mLockPitch,
  mPinToHand,
  mPinnedHand,
  mHeadOffsetX,
  mHeadOffsetY,
  mHeadOffsetZ,
  mHeadPitchDeg,
  mHeadYawDeg,
  mHandOffsetX,
  mHandOffsetY,
  mHandOffsetZ,
  mHandPitchDeg,
  mHandForwardX,
  mHandForwardY,
  mHandForwardZ,
  mWristTiltOffset)
VDXROVR_DEFINE_SPARSE_JSON(
  VRGestureSettings,
  mEnabled,
  mAllowPinchReposition,
  mPinchThresholdM,
  mSwipeMinDistanceM,
  mCooldownMs)
NLOHMANN_JSON_SERIALIZE_ENUM(
  VRMirrorEyeMode,
  {
    {VRMirrorEyeMode::Left, "Left"},
    {VRMirrorEyeMode::Right, "Right"},
    {VRMirrorEyeMode::Both, "Both"},
  });
NLOHMANN_JSON_SERIALIZE_ENUM(
  VRMirrorResolution,
  {
    {VRMirrorResolution::HD720, "HD720"},
    {VRMirrorResolution::FHD1080, "FHD1080"},
  });
VDXROVR_DEFINE_SPARSE_JSON(
  VRMirrorSettings,
  mEnabled,
  mEyeMode,
  mResolution,
  mSmoothingEnabled,
  mTemporalStrength,
  mCameraStabilization,
  mShowOverlays,
  mIpdMm,
  mCaptureGameAudio,
  mWindowAudioMonitor)
VDXROVR_DEFINE_SPARSE_JSON(
  VRRenderSettings,
  mQuirks,
  mEnableGazeInputFocus,
  mEditModeEnabled,
  mAllowControllerManipulation,
  mHeadFollow,
  mGestures,
  mMirror)

template <>
void from_json_postprocess<VRSettings>(const nlohmann::json& j, VRSettings& v) {
  from_json(j, static_cast<VRRenderSettings&>(v));
  from_json(j, v.mDeprecated.mPrimaryLayer);

  // Backwards compatibility
  if (j.contains("height")) {
    v.mDeprecated.mMaxHeight = j.at("height");
  }
  if (j.contains("width")) {
    v.mDeprecated.mMaxWidth = j.at("width");
  }
}

template <>
void to_json_postprocess<VRSettings>(
  nlohmann::json& j,
  const VRSettings& parent_v,
  const VRSettings& v) {
  to_json_with_default(
    j,
    static_cast<const VRRenderSettings&>(parent_v),
    static_cast<const VRRenderSettings&>(v));
  to_json_with_default(
    j, parent_v.mDeprecated.mPrimaryLayer, v.mDeprecated.mPrimaryLayer);
}

VDXROVR_DEFINE_SPARSE_JSON(
  VRSettings::Deprecated,
  mMaxWidth,
  mMaxHeight,
  mEnableGazeZoom,
  mZoomScale,
  mGazeTargetScale,
  mOpacity)
VDXROVR_DEFINE_SPARSE_JSON(VRSettings, mEnableSteamVR)

}// namespace VDXROVR
