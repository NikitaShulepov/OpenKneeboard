// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <VDXROVR/ToolbarAction.hpp>

namespace VDXROVR {

ToolbarAction::ToolbarAction(std::string glyph, std::string label)
  : mGlyph(glyph),
    mLabel(label) {}

ToolbarAction::~ToolbarAction() {}

std::string_view ToolbarAction::GetGlyph() const { return mGlyph; }

std::string_view ToolbarAction::GetLabel() const { return mLabel; }

}// namespace VDXROVR
