// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <string>

namespace VDXROVR::Shaders::Viewer::DXBC::Detail {

#include <VDXROVR/Shaders/gen/VDXROVR-Viewer-DXBC-PS.hpp>
#include <VDXROVR/Shaders/gen/VDXROVR-Viewer-DXBC-VS.hpp>

}// namespace VDXROVR::Shaders::Viewer::DXBC::Detail

namespace VDXROVR::Shaders::Viewer::DXBC {

constexpr std::basic_string_view<unsigned char> PS {
  Detail::g_ViewerPixelShader,
  std::size(Detail::g_ViewerPixelShader)};

constexpr std::basic_string_view<unsigned char> VS {
  Detail::g_ViewerVertexShader,
  std::size(Detail::g_ViewerVertexShader)};

}// namespace VDXROVR::Shaders::Viewer::DXBC
