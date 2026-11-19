// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
// clang-format off
#include "pch.h"
#include "AppWindowViewModeDialog.xaml.h"
#include "AppWindowViewModeDialog.g.cpp"
// clang-format on

#include <VDXROVR/ViewsSettings.hpp>

#include <VDXROVR/utf8.hpp>

#include <format>

using namespace VDXROVR;

namespace winrt::VDXROVRApp::implementation {

AppWindowViewModeDialog::AppWindowViewModeDialog() { InitializeComponent(); }

AppWindowViewModeDialog::~AppWindowViewModeDialog() = default;

uint8_t AppWindowViewModeDialog::SelectedMode(
  ContentDialogResult result) const noexcept {
  using VDXROVR::AppWindowViewMode;
  switch (result) {
    case ContentDialogResult::Primary:
      return static_cast<uint8_t>(AppWindowViewMode::ActiveView);
    case ContentDialogResult::Secondary:
      return static_cast<uint8_t>(AppWindowViewMode::Independent);
    default:
      return static_cast<uint8_t>(AppWindowViewMode::NoDecision);
  }
}

}// namespace winrt::VDXROVRApp::implementation
