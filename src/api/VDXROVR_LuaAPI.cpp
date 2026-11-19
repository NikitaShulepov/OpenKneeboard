// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <VDXROVR/APIEvent.hpp>

#include <VDXROVR/dprint.hpp>
#include <VDXROVR/tracing.hpp>

#include <shims/winrt/base.h>

#include <Windows.h>

#include <cinttypes>
#include <cstdlib>
#include <format>
#include <string>

extern "C" {
#include <lauxlib.h>
}

using VDXROVR::dprint;

namespace VDXROVR {

/* PS >
 * [System.Diagnostics.Tracing.EventSource]::new("VDXROVR.API.Lua")
 * 32de7b33-5379-4458-bee6-e3260db8208a
 */
TRACELOGGING_DEFINE_PROVIDER(
  gTraceProvider,
  "VDXROVR.API.Lua",
  (0x32de7b33, 0x5379, 0x4458, 0xbe, 0xe6, 0xe3, 0x26, 0x0d, 0xb8, 0x20, 0x8a));
}// namespace VDXROVR

static void push_arg_error(lua_State* state) {
  lua_pushliteral(state, "2 string arguments are required\n");
  lua_error(state);
}

static int SendToVDXROVR(lua_State* state) {
  VDXROVR_TraceLoggingScopedActivity(activity, "SendToVDXROVR");
  int argc = lua_gettop(state);
  if (argc != 2) {
    dprint("Invalid argument count\n");
    push_arg_error(state);
    activity.StopWithResult("InvalidArgs");
    return 1;
  }

  if (!(lua_isstring(state, 1) && lua_isstring(state, 2))) {
    dprint("Non-string args\n");
    push_arg_error(state);
    activity.StopWithResult("NonStringArgs");
    return 1;
  }

  const VDXROVR::APIEvent event {
    lua_tostring(state, 1),
    lua_tostring(state, 2),
  };
  event.Send();

  return 0;
}

extern "C" int __declspec(dllexport)
#if UINTPTR_MAX == UINT64_MAX
luaopen_VDXROVR_LuaAPI64(lua_State* state) {
#elif UINTPTR_MAX == UINT32_MAX
luaopen_VDXROVR_LuaAPI32(lua_State* state) {
#endif
  VDXROVR_TraceLoggingScope("luaopen_VDXROVR_LuaAPI64");
  VDXROVR::DPrintSettings::Set({
    .prefix = "VDXROVR-LuaAPI",
  });
  lua_createtable(state, 0, 1);
  lua_pushcfunction(state, &SendToVDXROVR);
  lua_setfield(state, -2, "sendRaw");
  return 1;
}

BOOL WINAPI DllMain(HINSTANCE, const DWORD dwReason, LPVOID /* lpReserved */) {
  const auto& provider = VDXROVR::gTraceProvider;
  switch (dwReason) {
    case DLL_PROCESS_ATTACH:
      TraceLoggingRegister(provider);
      TraceLoggingWrite(provider, "Attached", TraceLoggingThisExecutable());
      break;
    case DLL_PROCESS_DETACH:
      TraceLoggingWrite(provider, "Detached", TraceLoggingThisExecutable());
      TraceLoggingUnregister(provider);
      break;
  }
  return TRUE;
}
