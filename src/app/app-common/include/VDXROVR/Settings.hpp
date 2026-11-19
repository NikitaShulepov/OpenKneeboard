// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <VDXROVR/AppSettings.hpp>
#include <VDXROVR/DirectInputSettings.hpp>
#include <VDXROVR/DoodleSettings.hpp>
#include <VDXROVR/TabletSettings.hpp>
#include <VDXROVR/TextSettings.hpp>
#include <VDXROVR/UISettings.hpp>
#include <VDXROVR/VRSettings.hpp>
#include <VDXROVR/ViewsSettings.hpp>

#include <filesystem>

namespace VDXROVR {

#define VDXROVR_GLOBAL_SETTINGS_SECTIONS IT(AppSettings, App)

#define VDXROVR_PER_PROFILE_SETTINGS_SECTIONS \
  IT(DirectInputSettings, DirectInput) \
  IT(DoodleSettings, Doodles) \
  IT(TextSettings, Text) \
  IT(TabletSettings, TabletInput) \
  IT(nlohmann::json, Tabs) \
  IT(UISettings, UI) \
  IT(ViewsSettings, Views) \
  IT(VRSettings, VR)

#define VDXROVR_SETTINGS_SECTIONS \
  VDXROVR_GLOBAL_SETTINGS_SECTIONS \
  VDXROVR_PER_PROFILE_SETTINGS_SECTIONS

struct Settings final {
#define IT(cpptype, name) cpptype m##name {};
  VDXROVR_SETTINGS_SECTIONS
#undef IT

  static Settings Load(winrt::guid defaultProfile, winrt::guid activeProfile);
  void Save(winrt::guid defaultProfile, winrt::guid activeProfile) const;
#define IT(cpptype, name) \
  void Reset##name##Section( \
    winrt::guid defaultProfile, winrt::guid activeProfile);
  VDXROVR_SETTINGS_SECTIONS
#undef IT

  bool operator==(const Settings&) const noexcept = default;
};

VDXROVR_DECLARE_SPARSE_JSON(Settings);

}// namespace VDXROVR
