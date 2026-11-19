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

#include "HelpPage.g.h"
#include "WithPropertyChangedEvent.h"

#include <VDXROVR/Events.hpp>

#include <filesystem>
#include <string>

using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::VDXROVRApp::implementation {
struct HelpPage : HelpPageT<HelpPage>,
                  private VDXROVR::EventReceiver,
                  public VDXROVR::WithPropertyChangedEvent {
  HelpPage();
  ~HelpPage();

  void OnCopyVersionDataClick(
    const IInspectable&,
    const RoutedEventArgs&) noexcept;
  void OnAgreeClick(const IInspectable&, const RoutedEventArgs&) noexcept;
  VDXROVR::fire_and_forget OnExportClick(
    IInspectable,
    RoutedEventArgs) noexcept;

  VDXROVR::fire_and_forget OnCheckForUpdatesClick(
    IInspectable,
    RoutedEventArgs) noexcept;

  bool AgreedToPrivacyWarning() noexcept;
  bool AgreeButtonIsEnabled() noexcept;

 private:
  winrt::apartment_context mUIThread;
  std::string mVersionClipboardData;

  void PopulateVersion();
  void PopulateLicenses() noexcept;

  static std::string GetUpdateLog() noexcept;
  static std::string GetOpenXRInfo() noexcept;
  static std::string GetActiveConsumers() noexcept;
  static std::string GetVRAMInfo() noexcept;

  void DisplayLicense(const std::string& header, const std::filesystem::path&);

  static bool mAgreedToPrivacyWarning;
};
}// namespace winrt::VDXROVRApp::implementation
namespace winrt::VDXROVRApp::factory_implementation {
struct HelpPage : HelpPageT<HelpPage, implementation::HelpPage> {};
}// namespace winrt::VDXROVRApp::factory_implementation
