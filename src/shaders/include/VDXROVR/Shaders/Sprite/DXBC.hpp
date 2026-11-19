// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <string>

namespace VDXROVR::Shaders::Sprite::DXBC::Detail {

#include <VDXROVR/Shaders/gen/VDXROVR-Sprite-DXBC-PS.hpp>
#include <VDXROVR/Shaders/gen/VDXROVR-Sprite-DXBC-VS.hpp>

}// namespace VDXROVR::Shaders::Sprite::DXBC::Detail

namespace VDXROVR::Shaders::Sprite::DXBC {

constexpr std::basic_string_view<unsigned char> PS {
  Detail::g_SpritePixelShader,
  std::size(Detail::g_SpritePixelShader)};

constexpr std::basic_string_view<unsigned char> VS {
  Detail::g_SpriteVertexShader,
  std::size(Detail::g_SpriteVertexShader)};

}// namespace VDXROVR::Shaders::Sprite::DXBC
