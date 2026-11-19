// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <VDXROVR/Events.hpp>
#include <VDXROVR/Plugin.hpp>

namespace VDXROVR {

class PluginStore final {
 public:
  PluginStore();
  ~PluginStore();

  std::vector<Plugin> GetPlugins() const noexcept;
  std::vector<Plugin::TabType> GetTabTypes() const noexcept;
  void Append(const Plugin&);

  Event<> evTabTypesChanged;

 private:
  std::vector<Plugin> mPlugins;

  void LoadPluginsFromFilesystem();
  void LoadPluginsFromRegistry();
  void LoadPluginsFromRegistry(HKEY root);

  void TryAppend(const std::filesystem::path&);
};

}// namespace VDXROVR
