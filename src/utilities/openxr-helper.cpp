// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.

/** A separate process to register the OpenXR layer, outside of the
 * MSIX sandbox.
 *
 * If done from the main process, the registry write will be app-specific.
 */

#include <VDXROVR/RuntimeFiles.hpp>

#include <VDXROVR/dprint.hpp>
#include <VDXROVR/scope_exit.hpp>

#include <Windows.h>
#include <shellapi.h>

#include <filesystem>
#include <functional>
#include <string>

using namespace VDXROVR;

enum class RegistryView {
  WOW64_64,
  WOW64_32,
};

static HKEY OpenOrCreateImplicitLayerRegistryKey(RegistryView view, HKEY root) {
  HKEY openXRKey {0};
  const auto result = RegCreateKeyExW(
    root,
    L"SOFTWARE\\Khronos\\OpenXR\\1\\ApiLayers\\Implicit",
    0,
    nullptr,
    0,
    KEY_ALL_ACCESS
      | ((view == RegistryView::WOW64_64) ? KEY_WOW64_64KEY : KEY_WOW64_32KEY),
    nullptr,
    &openXRKey,
    nullptr);
  if (result != ERROR_SUCCESS) {
    dprint("Failed to open OpenXR implicit layer key: {}", result);
  }
  return openXRKey;
}

static void DisableOpenXRLayers(
  RegistryView view,
  HKEY root,
  std::function<bool(std::wstring_view)> predicate) {
  auto openXRKey = OpenOrCreateImplicitLayerRegistryKey(view, root);
  if (!openXRKey) {
    return;
  }

  // https://docs.microsoft.com/en-us/windows/win32/sysinfo/registry-element-size-limits
  std::wstring valueNameBuffer(16383, L'x');
  DWORD valueSize = valueNameBuffer.size();
  DWORD valueIndex {0};
  DWORD disabled = 1;
  while (RegEnumValueW(
           openXRKey,
           valueIndex++,
           valueNameBuffer.data(),
           &valueSize,
           nullptr,
           nullptr,
           nullptr,
           nullptr)
         == ERROR_SUCCESS) {
    std::wstring valueName = valueNameBuffer.substr(0, valueSize);
    valueSize = valueNameBuffer.size();

    if (predicate(valueName)) {
      RegSetValueExW(
        openXRKey,
        valueName.c_str(),
        0,
        REG_DWORD,
        reinterpret_cast<const BYTE*>(&disabled),
        sizeof(disabled));
    }
  }

  RegCloseKey(openXRKey);
}

static void DisableOpenXRLayer(
  RegistryView view,
  HKEY root,
  const std::filesystem::path& rawJsonPath) {
  const auto jsonPath = std::filesystem::canonical(rawJsonPath).wstring();

  DisableOpenXRLayers(view, root, [jsonPath](std::wstring_view layerPath) {
    return layerPath == jsonPath;
  });
}

static void EnableOpenXRLayer(
  RegistryView view,
  HKEY root,
  const std::filesystem::path& rawJsonPath) {
  auto openXRKey = OpenOrCreateImplicitLayerRegistryKey(view, root);
  if (!openXRKey) {
    dprint("Failed to open or create OpenXR key");
    return;
  }

  const auto jsonPath = std::filesystem::canonical(rawJsonPath).wstring();
  const auto jsonFile = rawJsonPath.filename().wstring();
  DisableOpenXRLayers(
    view, root, [jsonFile, jsonPath](std::wstring_view layerPath) {
      return layerPath != jsonPath && layerPath.ends_with(jsonFile);
    });

  DWORD disabled = 0;
  const auto success = RegSetValueExW(
    openXRKey,
    jsonPath.c_str(),
    0,
    REG_DWORD,
    reinterpret_cast<const BYTE*>(&disabled),
    sizeof(disabled));
  if (success != ERROR_SUCCESS) {
    dprint("Failed to set OpenXR key: {}", success);
  }

  RegCloseKey(openXRKey);
}

namespace VDXROVR {

/* PS >
 * [System.Diagnostics.Tracing.EventSource]::new("VDXROVR.OpenXR.Helper")
 * 88b84e86-e559-414d-8738-c59f86dcbf08
 */
TRACELOGGING_DEFINE_PROVIDER(
  gTraceProvider,
  "VDXROVR.OpenXR.Helper",
  (0x88b84e86, 0xe559, 0x414d, 0x87, 0x38, 0xc5, 0x9f, 0x86, 0xdc, 0xbf, 0x08));
}// namespace VDXROVR

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR commandLine, int) {
  TraceLoggingRegister(gTraceProvider);
  const scope_exit unregisterTraceProvider(
    []() { TraceLoggingUnregister(gTraceProvider); });
  DPrintSettings::Set({
    .prefix = "OpenXR-Helper",
    .consoleOutput = DPrintSettings::ConsoleOutputMode::ALWAYS,
  });
  int argc = 0;
  auto argv = CommandLineToArgvW(commandLine, &argc);
  if (argc != 2) {
    dprint("Invalid arguments ({}):", argc);
    for (int i = 0; i < argc; ++i) {
      dprint(L"argv[{}]: {}", i, argv[i]);
    }
    return 1;
  }
  dprint(L"OpenXR: {} -> {}", argv[0], argv[1]);
  const auto command = std::wstring_view(argv[0]);
  const std::filesystem::path directory {std::wstring_view(argv[1])};

  const auto layer64 = directory / RuntimeFiles::OPENXR_64BIT_JSON;
  if (command == L"disable-HKLM-64") {
    DisableOpenXRLayer(RegistryView::WOW64_64, HKEY_LOCAL_MACHINE, layer64);
    return 0;
  }
  if (command == L"enable-HKLM-64") {
    EnableOpenXRLayer(RegistryView::WOW64_64, HKEY_LOCAL_MACHINE, layer64);
    return 0;
  }

  const auto layer32 = directory / RuntimeFiles::OPENXR_32BIT_JSON;
  if (command == L"disable-HKLM-32") {
    DisableOpenXRLayer(RegistryView::WOW64_32, HKEY_LOCAL_MACHINE, layer32);
    return 0;
  }
  if (command == L"enable-HKLM-32") {
    EnableOpenXRLayer(RegistryView::WOW64_32, HKEY_LOCAL_MACHINE, layer32);
    return 0;
  }

  dprint(L"Invalid command: {}", command);
  return 1;
}
