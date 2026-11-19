// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

// clang-format off
#include "pch.h"
#include "InputSettingsPage.g.h"
// clang-format on

#include "WithPropertyChangedEvent.h"

#include <VDXROVR/Events.hpp>

#include <string>

using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Windows::Foundation::Collections;

namespace VDXROVR {
class KneeboardState;
}

namespace winrt::VDXROVRApp::implementation {
struct InputSettingsPage
  : InputSettingsPageT<InputSettingsPage>,
    VDXROVR::WithPropertyChangedEventOnProfileChange<InputSettingsPage> {
  InputSettingsPage();
  ~InputSettingsPage();

  VDXROVR::fire_and_forget RestoreDefaults(
    IInspectable,
    RoutedEventArgs) noexcept;

  IVector<IInspectable> Devices() noexcept;
  void OnOrientationChanged(
    const IInspectable&,
    const SelectionChangedEventArgs&);

 private:
  winrt::apartment_context mUIThread;
  VDXROVR::audited_ptr<VDXROVR::KneeboardState> mKneeboard;
};
}// namespace winrt::VDXROVRApp::implementation
namespace winrt::VDXROVRApp::factory_implementation {
struct InputSettingsPage
  : InputSettingsPageT<InputSettingsPage, implementation::InputSettingsPage> {};
}// namespace winrt::VDXROVRApp::factory_implementation
