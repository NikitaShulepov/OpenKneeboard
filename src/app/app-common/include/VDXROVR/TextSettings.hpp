// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <VDXROVR/json_fwd.hpp>

namespace VDXROVR {
struct TextSettings final {
  float mFontSize = 20.f;

  constexpr bool operator==(const TextSettings&) const = default;
};

VDXROVR_DECLARE_SPARSE_JSON(TextSettings)

}// namespace VDXROVR
