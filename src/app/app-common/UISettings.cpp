// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <VDXROVR/AppSettings.hpp>

#include <VDXROVR/json.hpp>

namespace VDXROVR {

VDXROVR_DEFINE_SPARSE_JSON(BookmarkSettings, mEnabled, mLoop)
VDXROVR_DEFINE_SPARSE_JSON(
  InGameUISettings,
  mHeaderEnabled,
  mFooterEnabled,
  mFooterFrameCountEnabled,
  mBookmarksBarEnabled);

VDXROVR_DEFINE_SPARSE_JSON(
  TintSettings,
  mEnabled,
  mBrightness,
  mBrightnessStep,
  mRed,
  mGreen,
  mBlue)

// mWindowRect is handled by `*_json_postprocess` functions above
VDXROVR_DEFINE_SPARSE_JSON(
  UISettings,
  mLoopPages,
  mLoopTabs,
  mBookmarks,
  mInGameUI,
  mTint)

}// namespace VDXROVR
