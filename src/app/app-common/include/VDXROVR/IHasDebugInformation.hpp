// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <VDXROVR/Events.hpp>

#include <string>

namespace VDXROVR {

class IHasDebugInformation {
 public:
  virtual ~IHasDebugInformation();

  virtual std::string GetDebugInformation() const = 0;

  Event<std::string> evDebugInformationHasChanged;
};

}// namespace VDXROVR
