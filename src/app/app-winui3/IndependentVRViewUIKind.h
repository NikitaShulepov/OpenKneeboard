// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

// clang-format off
#include "pch.h"
#include "IndependentVRViewUIKind.g.h"
#include "UIDataItem.h"
// clang-format on

namespace winrt::VDXROVRApp::implementation {
struct IndependentVRViewUIKind
  : IndependentVRViewUIKindT<
      IndependentVRViewUIKind,
      VDXROVRApp::implementation::UIDataItem> {
  IndependentVRViewUIKind();
};
}// namespace winrt::VDXROVRApp::implementation

namespace winrt::VDXROVRApp::factory_implementation {
struct IndependentVRViewUIKind : IndependentVRViewUIKindT<
                                   IndependentVRViewUIKind,
                                   implementation::IndependentVRViewUIKind> {};
}// namespace winrt::VDXROVRApp::factory_implementation
