// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <VDXROVR/Events.hpp>

#include <VDXROVR/final_release_deleter.hpp>

#include <shims/winrt/base.h>

#include <winrt/Windows.Storage.Search.h>

#include <wil/resource.h>

#include <filesystem>
#include <memory>

#include <fileapi.h>

namespace VDXROVR {

class FilesystemWatcher final
  : public std::enable_shared_from_this<FilesystemWatcher> {
 public:
  static std::shared_ptr<FilesystemWatcher> Create(
    const std::filesystem::path&);

  static VDXROVR::fire_and_forget final_release(
    std::unique_ptr<FilesystemWatcher>);

  Event<std::filesystem::path> evFilesystemModifiedEvent;

  FilesystemWatcher() = delete;
  ~FilesystemWatcher();

 private:
  FilesystemWatcher(const std::filesystem::path&);
  void Initialize();
  task<void> Run();
  VDXROVR::fire_and_forget OnContentsChanged();

  std::optional<task<void>> mImpl;

  winrt::apartment_context mOwnerThread;
  std::filesystem::path mPath;

  std::filesystem::file_time_type mLastWriteTime;
  bool mSettling = false;

  wil::unique_hfind_change mHandle {};
  std::stop_source mStop;
};

}// namespace VDXROVR
