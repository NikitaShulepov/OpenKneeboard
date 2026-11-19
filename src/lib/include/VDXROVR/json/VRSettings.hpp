// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <VDXROVR/json_fwd.hpp>

namespace VDXROVR {

struct GazeTargetScale;
struct VROpacitySettings;
struct VRPose;
struct VRSettings;

VDXROVR_DECLARE_SPARSE_JSON(GazeTargetScale)
VDXROVR_DECLARE_SPARSE_JSON(VROpacitySettings)
VDXROVR_DECLARE_SPARSE_JSON(VRPose)
VDXROVR_DECLARE_SPARSE_JSON(VRSettings)

}// namespace VDXROVR
