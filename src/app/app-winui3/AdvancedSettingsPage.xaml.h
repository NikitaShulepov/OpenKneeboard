// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

// clang-format off
#include "pch.h"
// clang-format on

#include "AdvancedSettingsPage.g.h"
#include "WithPropertyChangedEvent.h"

#include <VDXROVR/Events.hpp>

using namespace winrt::Microsoft::UI::Xaml;

namespace VDXROVR {
class KneeboardState;
};

namespace winrt::VDXROVRApp::implementation {
struct AdvancedSettingsPage
  : AdvancedSettingsPageT<AdvancedSettingsPage>,
    VDXROVR::WithPropertyChangedEventOnProfileChange<
      AdvancedSettingsPage> {
  AdvancedSettingsPage();
  ~AdvancedSettingsPage();

  bool MultipleProfiles() const noexcept;
  VDXROVR::fire_and_forget MultipleProfiles(bool value) noexcept;

  bool Bookmarks() const noexcept;
  VDXROVR::fire_and_forget Bookmarks(bool value) noexcept;

  uint8_t AppWindowViewMode() const noexcept;
  VDXROVR::fire_and_forget AppWindowViewMode(uint8_t value) noexcept;

  bool EnableMouseButtonBindings() const noexcept;
  VDXROVR::fire_and_forget EnableMouseButtonBindings(bool value) noexcept;

  bool GazeInputFocus() const noexcept;
  VDXROVR::fire_and_forget GazeInputFocus(bool value) noexcept;

  bool LoopPages() const noexcept;
  VDXROVR::fire_and_forget LoopPages(bool) noexcept;

  bool LoopTabs() const noexcept;
  VDXROVR::fire_and_forget LoopTabs(bool) noexcept;

  bool LoopProfiles() const noexcept;
  VDXROVR::fire_and_forget LoopProfiles(bool) noexcept;

  bool LoopBookmarks() const noexcept;
  VDXROVR::fire_and_forget LoopBookmarks(bool) noexcept;

  bool InGameHeader() const noexcept;
  VDXROVR::fire_and_forget InGameHeader(bool) noexcept;

  bool InGameFooter() const noexcept;
  VDXROVR::fire_and_forget InGameFooter(bool) noexcept;

  bool InGameFooterFrameCount() const noexcept;
  VDXROVR::fire_and_forget InGameFooterFrameCount(bool) noexcept;

  VDXROVR::fire_and_forget RestoreDoodleDefaults(
    Windows::Foundation::IInspectable,
    Windows::Foundation::IInspectable) noexcept;
  VDXROVR::fire_and_forget RestoreTextDefaults(
    Windows::Foundation::IInspectable,
    Windows::Foundation::IInspectable) noexcept;
  VDXROVR::fire_and_forget RestoreQuirkDefaults(
    Windows::Foundation::IInspectable,
    Windows::Foundation::IInspectable) noexcept;

  uint32_t MinimumPenRadius();
  VDXROVR::fire_and_forget MinimumPenRadius(uint32_t value);
  uint32_t PenSensitivity();
  VDXROVR::fire_and_forget PenSensitivity(uint32_t value);

  uint32_t MinimumEraseRadius();
  VDXROVR::fire_and_forget MinimumEraseRadius(uint32_t value);
  uint32_t EraseSensitivity();
  VDXROVR::fire_and_forget EraseSensitivity(uint32_t value);

  float TextPageFontSize();
  VDXROVR::fire_and_forget TextPageFontSize(float value);

  bool TintEnabled();
  VDXROVR::fire_and_forget TintEnabled(bool value);
  float TintBrightness();
  VDXROVR::fire_and_forget TintBrightness(float value);
  winrt::Windows::UI::Color Tint();
  VDXROVR::fire_and_forget Tint(winrt::Windows::UI::Color value);

  uint8_t Quirk_OpenXR_Upscaling() const noexcept;
  VDXROVR::fire_and_forget Quirk_OpenXR_Upscaling(uint8_t value) noexcept;

 private:
  winrt::apartment_context mUIThread {};
  VDXROVR::audited_ptr<VDXROVR::KneeboardState> mKneeboard;
};
}// namespace winrt::VDXROVRApp::implementation
namespace winrt::VDXROVRApp::factory_implementation {
struct AdvancedSettingsPage : AdvancedSettingsPageT<
                                AdvancedSettingsPage,
                                implementation::AdvancedSettingsPage> {};
}// namespace winrt::VDXROVRApp::factory_implementation
