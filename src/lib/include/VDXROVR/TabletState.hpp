// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <cinttypes>
#include <string>

namespace VDXROVR {

struct TabletState final {
  bool mIsActive = false;
  float mX {};
  float mY {};
  uint32_t mPressure {};
  uint32_t mPenButtons {};
  uint32_t mAuxButtons {};
};

}// namespace VDXROVR
