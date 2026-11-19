/*
 * VDXROVR
 *
 * Copyright (C) 2022-2023 Fred Emmott <fred@fredemmott.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

#define VDXROVR_CAPI_IMPL
#include "VDXROVR_CAPI.h"

#include <VDXROVR/APIEvent.hpp>

#include <VDXROVR/dprint.hpp>
#include <VDXROVR/tracing.hpp>

static void init() {
  VDXROVR::DPrintSettings::Set({
    .prefix = "VDXROVR-CAPI",
  });

  wchar_t buf[1024];
  const auto length = GetModuleFileNameW(NULL, buf, std::size(buf));
  if (length) {
    VDXROVR::dprint(
      L"new API client: {}", std::wstring_view {buf, length});
  } else {
    VDXROVR::dprint(
      "new API client - failed to get client path: {:#018x}",
      static_cast<uint64_t>(GetLastError()));
  }
}

VDXROVR_CAPI void VDXROVR_send_utf8(
  const char* eventName,
  size_t eventNameByteCount,
  const char* eventValue,
  size_t eventValueByteCount) {
  const VDXROVR::APIEvent ge {
    {eventName, eventNameByteCount},
    {eventValue, eventValueByteCount},
  };
  ge.Send();
}

VDXROVR_CAPI void VDXROVR_send_wchar_ptr(
  const wchar_t* eventName,
  size_t eventNameCharCount,
  const wchar_t* eventValue,
  size_t eventValueCharCount) {
  const VDXROVR::APIEvent ge {
    winrt::to_string(std::wstring_view {eventName, eventNameCharCount}),
    winrt::to_string(std::wstring_view {eventValue, eventValueCharCount}),
  };
  ge.Send();
}

namespace VDXROVR {

/* PS >
 * [System.Diagnostics.Tracing.EventSource]::new("VDXROVR.API.C")
 * 1d453b62-4afe-4cc2-a91b-0322134dce7d
 */
TRACELOGGING_DEFINE_PROVIDER(
  gTraceProvider,
  "VDXROVR.API.C",
  (0x1d453b62, 0x4afe, 0x4cc2, 0xa9, 0x1b, 0x03, 0x22, 0x13, 0x4d, 0xce, 0x7d));
}// namespace VDXROVR

BOOL WINAPI DllMain(HINSTANCE, const DWORD dwReason, LPVOID /* lpReserved */) {
  switch (dwReason) {
    case DLL_PROCESS_ATTACH:
      TraceLoggingRegister(VDXROVR::gTraceProvider);
      TraceLoggingWrite(
        VDXROVR::gTraceProvider,
        "Attached",
        TraceLoggingThisExecutable());
      init();
      break;
    case DLL_PROCESS_DETACH:
      TraceLoggingWrite(
        VDXROVR::gTraceProvider,
        "Detached",
        TraceLoggingThisExecutable());
      TraceLoggingUnregister(VDXROVR::gTraceProvider);
      break;
  }
  return TRUE;
}
