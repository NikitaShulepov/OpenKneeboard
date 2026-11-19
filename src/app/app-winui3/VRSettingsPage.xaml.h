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

  VDXROVR::fire_and_forget RestoreDefaults(
    IInspectable,
    RoutedEventArgs) noexcept;

  VDXROVR::fire_and_forget AddView(muxc::TabView, IInspectable) noexcept;

  VDXROVR::fire_and_forget RemoveView(
    muxc::TabView,
    muxc::TabViewTabCloseRequestedEventArgs) noexcept;

 private:
  VDXROVR::audited_ptr<KneeboardState> mKneeboard;
  void PopulateViews() noexcept;

  void AppendViewTab(const ViewSettings& view) noexcept;
};
}// namespace winrt::VDXROVRApp::implementation
namespace winrt::VDXROVRApp::factory_implementation {
struct VRSettingsPage
  : VRSettingsPageT<VRSettingsPage, implementation::VRSettingsPage> {};
}// namespace winrt::VDXROVRApp::factory_implementation
