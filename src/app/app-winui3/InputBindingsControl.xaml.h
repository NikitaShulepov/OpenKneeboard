// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

// clang-format off
#include "pch.h"
#include "InputBindingsControl.g.h"
// clang-format on

#include <VDXROVR/Events.hpp>

#include <unordered_map>

namespace VDXROVR {
class UserInputDevice;
enum class UserAction;
}// namespace VDXROVR

using namespace VDXROVR;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::VDXROVRApp::implementation {
struct InputBindingsControl : InputBindingsControlT<InputBindingsControl> {
  InputBindingsControl() noexcept;
  ~InputBindingsControl();

  hstring DeviceID();
  void DeviceID(const hstring&);

 private:
  winrt::apartment_context mUIThread;
  ::VDXROVR::fire_and_forget PromptForBinding(UserAction);
  void ClearBinding(UserAction);

  void PopulateUI();
  void AppendUIRow(UserAction, winrt::hstring label);
  void UpdateUI();

  hstring mDeviceID;
  std::shared_ptr<UserInputDevice> mDevice;

  struct Row {
    TextBlock mCurrentBinding;
    Button mBindButton;
    Button mClearButton;
  };
  std::unordered_map<UserAction, Row> mRows;
  void UpdateUI(UserAction);
};
}// namespace winrt::VDXROVRApp::implementation
namespace winrt::VDXROVRApp::factory_implementation {
struct InputBindingsControl : InputBindingsControlT<
                                InputBindingsControl,
                                implementation::InputBindingsControl> {};
}// namespace winrt::VDXROVRApp::factory_implementation
