// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.

#include <VDXROVR/DoodleSettings.hpp>

#include <VDXROVR/json.hpp>

namespace VDXROVR {
VDXROVR_DEFINE_SPARSE_JSON(
  DoodleSettings::Tool,
  mMinimumRadius,
  mSensitivity)
VDXROVR_DEFINE_SPARSE_JSON(DoodleSettings, mPen, mEraser)

}// namespace VDXROVR
