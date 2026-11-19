// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <filesystem>
#include <string_view>

namespace VDXROVR::RuntimeFiles {

#ifdef _WIN64
#define VDXROVR_PRIVATE_ARCH_RUNTIME_FILES \
  IT(WINDOW_CAPTURE_HOOK_32BIT_HELPER)
#else
#define VDXROVR_PRIVATE_ARCH_RUNTIME_FILES
#endif

#define VDXROVR_PRIVATE_RUNTIME_FILES \
  IT(OPENXR_REGISTER_LAYER_HELPER) \
  VDXROVR_PRIVATE_ARCH_RUNTIME_FILES

#define VDXROVR_PUBLIC_RUNTIME_FILES \
  IT(CHROMIUM) \
  IT(DCSWORLD_HOOK_DLL) \
  IT(DCSWORLD_HOOK_LUA) \
  IT(WINDOW_CAPTURE_HOOK_DLL) \
  IT(OPENXR_64BIT_DLL) \
  IT(OPENXR_32BIT_DLL) \
  IT(OPENXR_64BIT_JSON) \
  IT(OPENXR_32BIT_JSON) \
  IT(QUICK_START_PDF)

#define VDXROVR_RUNTIME_FILES \
  VDXROVR_PUBLIC_RUNTIME_FILES \
  VDXROVR_PRIVATE_RUNTIME_FILES

#define IT(x) extern const std::string_view x;
VDXROVR_RUNTIME_FILES
#undef IT

std::filesystem::path GetInstallationDirectory();

}// namespace VDXROVR::RuntimeFiles
