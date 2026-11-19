// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <VDXROVR/Pixels.hpp>

#include <VDXROVR/config.hpp>

#include <filesystem>
#include <string>
#include <unordered_map>

namespace VDXROVR {

struct WebPageSourceSettings {
  PixelSize mInitialSize {Config::DefaultWebPagePixelSize};
  bool mExposeVDXROVRAPIs {true};
  bool mIntegrateWithSimHub {true};
  std::string mURI;
  bool mTransparentBackground {true};

  ///// NOT SAVED - JUST FOR INTERNAL USE (e.g. PluginTab) /////
  std::unordered_map<std::string, std::filesystem::path> mVirtualHosts;

  constexpr bool operator==(const WebPageSourceSettings&) const noexcept =
    default;
};
}// namespace VDXROVR
