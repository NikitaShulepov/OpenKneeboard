// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <VDXROVR/APIEvent.hpp>

#include <VDXROVR/dprint.hpp>
#include <VDXROVR/scope_exit.hpp>
#include <VDXROVR/tracing.hpp>

using namespace VDXROVR;

#include <VDXROVR/scope_exit.hpp>

#include <Windows.h>
#include <shellapi.h>

#include <cstdlib>

namespace VDXROVR {
/* PS >
 * [System.Diagnostics.Tracing.EventSource]::new("VDXROVR.PluginTabAction")
 * 994c8b69-91da-4013-813b-6582e26cb144
 */
TRACELOGGING_DEFINE_PROVIDER(
  gTraceProvider,
  "VDXROVR.PluginTabAction",
  (0x994c8b69, 0x91da, 0x4013, 0x81, 0x3b, 0x65, 0x82, 0xe2, 0x6c, 0xb1, 0x44));

}// namespace VDXROVR

// We only need a standard `main()` function, but using wWinMain prevents
// a window/task bar entry from temporarily appearing
int WINAPI wWinMain(
  HINSTANCE /*hInstance*/,
  HINSTANCE /*hPrevInstance*/,
  PWSTR /*pCmdLine*/,
  int /*nCmdShow*/) {
  TraceLoggingRegister(VDXROVR::gTraceProvider);
  TraceLoggingWrite(
    VDXROVR::gTraceProvider,
    "Invocation",
    TraceLoggingThisExecutable(),
    TraceLoggingValue(GetCommandLineW(), "Command Line"));
  VDXROVR::scope_exit etwGuard(
    []() { TraceLoggingUnregister(VDXROVR::gTraceProvider); });

  DPrintSettings::Set({
    .prefix = "Plugin-Tab-Action",
    .consoleOutput = DPrintSettings::ConsoleOutputMode::ALWAYS,
  });
  int argc = 0;
  auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);

  if (argc < 1 || argc > 2) {
    dprint("Usage: ACTION_ID [CUSTOM_DATA]");
    return 0;
  }

  PluginTabCustomActionEvent ev {.mActionID = winrt::to_string(argv[0])};

  if (argc >= 2) {
    try {
      ev.mExtraData = nlohmann::json::parse(winrt::to_string(argv[1]));
    } catch (const nlohmann::json::exception& e) {
      dprint("Failed to parse JSON customData: {}", e.what());
      return 1;
    }
  }

  const nlohmann::json payload = ev;

  APIEvent::Send({APIEvent::EVT_PLUGIN_TAB_CUSTOM_ACTION, payload.dump()});

  return 0;
}
