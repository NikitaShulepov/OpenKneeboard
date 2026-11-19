// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <VDXROVR/Pixels.hpp>

#include <VDXROVR/config.hpp>
#include <VDXROVR/inplace_vector.hpp>

#include <span>

namespace VDXROVR {

struct SpriteAtlas {
  SpriteAtlas() = delete;
  explicit SpriteAtlas(const std::span<PixelSize>& sprites);

  [[nodiscard]]
  PixelSize GetAtlasSize() const {
    return mAtlasSize;
  }

  [[nodiscard]]
  PixelRect GetRect(const uint8_t spriteIndex) const {
    return mSprites.at(spriteIndex);
  }

  [[nodiscard]]
  PixelPoint GetOffset(const uint8_t spriteIndex) const {
    return mSprites.at(spriteIndex).mOffset;
  }

 private:
  PixelSize mAtlasSize;
  inplace_vector<PixelRect, MaxViewCount> mSprites;
};

}// namespace VDXROVR
