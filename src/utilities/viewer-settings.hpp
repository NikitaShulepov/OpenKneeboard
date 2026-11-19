// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <VDXROVR/json_fwd.hpp>

#include <shims/winrt/base.h>

namespace VDXROVR {

enum class ViewerFillMode {
  Default,
  Checkerboard,
  Transparent,
};

#define VDXROVR_VIEWER_ALIGNMENTS \
  IT(TopLeft) \
  IT(Top) \
  IT(TopRight) \
  IT(Left) \
  IT(Center) \
  IT(Right) \
  IT(BottomLeft) \
  IT(Bottom) \
  IT(BottomRight)

consteval size_t ViewerAlignmentsCount() {
  size_t count = 0;
#define IT(x) ++count;
  VDXROVR_VIEWER_ALIGNMENTS;
#undef IT
  return count;
}

enum class ViewerAlignment {
#define IT(x) x,
  VDXROVR_VIEWER_ALIGNMENTS
#undef IT
};

struct ViewerSettings final {
  int mWindowWidth {768 / 2};
  int mWindowHeight {1024 / 2};
  int mWindowX {CW_USEDEFAULT};
  int mWindowY {CW_USEDEFAULT};

  bool mBorderless {false};
  bool mStreamerMode {false};

  ViewerFillMode mFillMode {ViewerFillMode::Default};
  ViewerAlignment mAlignment {ViewerAlignment::Center};

  static ViewerSettings Load();
  void Save();

  constexpr bool operator==(const ViewerSettings&) const noexcept = default;
};

VDXROVR_DECLARE_SPARSE_JSON(ViewerSettings)

}// namespace VDXROVR
