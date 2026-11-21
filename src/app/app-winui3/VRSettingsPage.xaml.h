// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once
// clang-format off
#include "pch.h"
#include "VRSettingsPage.g.h"
// clang-format on

#include "WithPropertyChangedEvent.h"

#include <VDXROVR/Events.hpp>
#include <VDXROVR/ViewsSettings.hpp>

#include <cstdint>

#include <winrt/Microsoft.UI.Xaml.Controls.h>

using namespace winrt::Microsoft::UI::Xaml;
namespace muxc = winrt::Microsoft::UI::Xaml::Controls;

namespace VDXROVR {
class KneeboardState;
}

using namespace VDXROVR;

namespace winrt::VDXROVRApp::implementation {
struct VRSettingsPage
  : VRSettingsPageT<VRSettingsPage>,
    WithPropertyChangedEventOnProfileChange<VRSettingsPage> {
  VRSettingsPage();
  ~VRSettingsPage();

  bool SteamVREnabled();
  VDXROVR::fire_and_forget SteamVREnabled(bool);

  bool OpenXR64Enabled() noexcept;
  VDXROVR::fire_and_forget OpenXR64Enabled(bool) noexcept;

  bool OpenXR32Enabled() noexcept;
  VDXROVR::fire_and_forget OpenXR32Enabled(bool) noexcept;

  bool PinToHand() noexcept;
  VDXROVR::fire_and_forget PinToHand(bool) noexcept;
  int32_t PinnedHandIndex() noexcept;
  VDXROVR::fire_and_forget PinnedHandIndex(int32_t) noexcept;
  bool VREditModeEnabled() noexcept;
  VDXROVR::fire_and_forget VREditModeEnabled(bool) noexcept;
  bool AllowControllerManipulation() noexcept;
  VDXROVR::fire_and_forget AllowControllerManipulation(bool) noexcept;

  int32_t FollowModeIndex() noexcept;
  VDXROVR::fire_and_forget FollowModeIndex(int32_t) noexcept;
  float FollowDeadzoneAngleDeg() noexcept;
  VDXROVR::fire_and_forget FollowDeadzoneAngleDeg(float) noexcept;
  float FollowDeadzoneDistanceM() noexcept;
  VDXROVR::fire_and_forget FollowDeadzoneDistanceM(float) noexcept;
  float FollowSpeed() noexcept;
  VDXROVR::fire_and_forget FollowSpeed(float) noexcept;
  bool FollowLockRoll() noexcept;
  VDXROVR::fire_and_forget FollowLockRoll(bool) noexcept;
  bool FollowLockPitch() noexcept;
  VDXROVR::fire_and_forget FollowLockPitch(bool) noexcept;

  bool IsHeadFollowing() noexcept;
  float HeadOffsetXCm() noexcept;
  VDXROVR::fire_and_forget HeadOffsetXCm(float) noexcept;
  float HeadOffsetYCm() noexcept;
  VDXROVR::fire_and_forget HeadOffsetYCm(float) noexcept;
  float HeadDistanceCm() noexcept;
  VDXROVR::fire_and_forget HeadDistanceCm(float) noexcept;
  float HeadPitchDeg() noexcept;
  VDXROVR::fire_and_forget HeadPitchDeg(float) noexcept;
  float HeadYawDeg() noexcept;
  VDXROVR::fire_and_forget HeadYawDeg(float) noexcept;
  VDXROVR::fire_and_forget CenterHeadOverlay(
    IInspectable,
    RoutedEventArgs) noexcept;
  VDXROVR::fire_and_forget PresetLowerRightHud(
    IInspectable,
    RoutedEventArgs) noexcept;

  bool IsHandMounted() noexcept;
  float HandOffsetX() noexcept;
  VDXROVR::fire_and_forget HandOffsetX(float) noexcept;
  float HandOffsetY() noexcept;
  VDXROVR::fire_and_forget HandOffsetY(float) noexcept;
  float HandOffsetZ() noexcept;
  VDXROVR::fire_and_forget HandOffsetZ(float) noexcept;
  float HandPitchDeg() noexcept;
  VDXROVR::fire_and_forget HandPitchDeg(float) noexcept;
  float WristTiltOffset() noexcept;
  VDXROVR::fire_and_forget WristTiltOffset(float) noexcept;

  bool GesturesEnabled() noexcept;
  VDXROVR::fire_and_forget GesturesEnabled(bool) noexcept;
  bool GesturesAllowPinchReposition() noexcept;
  VDXROVR::fire_and_forget GesturesAllowPinchReposition(bool) noexcept;
  float GesturePinchThresholdMm() noexcept;
  VDXROVR::fire_and_forget GesturePinchThresholdMm(float) noexcept;
  float GestureSwipeMinDistanceM() noexcept;
  VDXROVR::fire_and_forget GestureSwipeMinDistanceM(float) noexcept;
  float GestureCooldownMs() noexcept;
  VDXROVR::fire_and_forget GestureCooldownMs(float) noexcept;

  bool MirrorEnabled() noexcept;
  VDXROVR::fire_and_forget MirrorEnabled(bool) noexcept;
  int32_t MirrorEyeModeIndex() noexcept;
  VDXROVR::fire_and_forget MirrorEyeModeIndex(int32_t) noexcept;
  int32_t MirrorResolutionIndex() noexcept;
  VDXROVR::fire_and_forget MirrorResolutionIndex(int32_t) noexcept;
  bool MirrorSmoothingEnabled() noexcept;
  VDXROVR::fire_and_forget MirrorSmoothingEnabled(bool) noexcept;
  int32_t MirrorTemporalStrength() noexcept;
  VDXROVR::fire_and_forget MirrorTemporalStrength(int32_t) noexcept;
  int32_t MirrorCameraStabilization() noexcept;
  VDXROVR::fire_and_forget MirrorCameraStabilization(int32_t) noexcept;
  bool MirrorShowOverlays() noexcept;
  VDXROVR::fire_and_forget MirrorShowOverlays(bool) noexcept;
  int32_t MirrorIpdMm() noexcept;
  VDXROVR::fire_and_forget MirrorIpdMm(int32_t) noexcept;
  bool MirrorCaptureGameAudio() noexcept;
  VDXROVR::fire_and_forget MirrorCaptureGameAudio(bool) noexcept;
  bool MirrorWindowAudioMonitor() noexcept;
  VDXROVR::fire_and_forget MirrorWindowAudioMonitor(bool) noexcept;
  VDXROVR::fire_and_forget OpenMirrorWindow(
    IInspectable,
    RoutedEventArgs) noexcept;
  VDXROVR::fire_and_forget InstallObsPlugin(
    IInspectable,
    RoutedEventArgs) noexcept;

  VDXROVR::fire_and_forget RestoreDefaults(
    IInspectable,
    RoutedEventArgs) noexcept;

  VDXROVR::fire_and_forget AddView(muxc::TabView, IInspectable) noexcept;

  VDXROVR::fire_and_forget RemoveView(
    muxc::TabView,
    muxc::TabViewTabCloseRequestedEventArgs) noexcept;

 private:
  VDXROVR::audited_ptr<KneeboardState> mKneeboard;
  winrt::apartment_context mUIThread;
  void OnVRSettingsChanged() noexcept;
  void PopulateViews() noexcept;

  void AppendViewTab(const ViewSettings& view) noexcept;
};
}// namespace winrt::VDXROVRApp::implementation
namespace winrt::VDXROVRApp::factory_implementation {
struct VRSettingsPage
  : VRSettingsPageT<VRSettingsPage, implementation::VRSettingsPage> {};
}// namespace winrt::VDXROVRApp::factory_implementation
