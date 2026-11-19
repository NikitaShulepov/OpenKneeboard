// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <VDXROVR/Filesystem.hpp>
#include <VDXROVR/RuntimeFiles.hpp>

namespace VDXROVR::RuntimeFiles {

std::filesystem::path GetInstallationDirectory() {
  // When using MSIX, we needed to copy files outside the sandbox; now that
  // we're using MSI, we don't.
  return Filesystem::GetRuntimeDirectory();
}

}// namespace VDXROVR::RuntimeFiles
