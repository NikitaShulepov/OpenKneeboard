// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.

#include <VDXROVR/tracing.hpp>

#include <cstdlib>

namespace VDXROVR {

/* PS >
 * [System.Diagnostics.Tracing.EventSource]::new("VDXROVR.RemoteControl")
 * 6d66807e-e21a-4f88-bdb1-b4f0ae54ae27
 */
TRACELOGGING_DEFINE_PROVIDER(
  gTraceProvider,
  "VDXROVR.RemoteControl",
  (0x6d66807e, 0xe21a, 0x4f88, 0xbd, 0xb1, 0xb4, 0xf0, 0xae, 0x54, 0xae, 0x27));

}// namespace VDXROVR

namespace {

class TraceLoggingRegistration {
 public:
  TraceLoggingRegistration() {
    TraceLoggingRegister(VDXROVR::gTraceProvider);
    TraceLoggingWrite(
      VDXROVR::gTraceProvider,
      "Invocation/Start",
      TraceLoggingThisExecutable(),
      TraceLoggingValue(GetCommandLineW(), "Command Line"));
  }
  ~TraceLoggingRegistration() {
    TraceLoggingWrite(VDXROVR::gTraceProvider, "Invocation/Stop");
    TraceLoggingUnregister(VDXROVR::gTraceProvider);
  }

  TraceLoggingRegistration(const TraceLoggingRegistration&) = delete;
  TraceLoggingRegistration(TraceLoggingRegistration&&) = delete;
  TraceLoggingRegistration& operator=(const TraceLoggingRegistration&) = delete;
  TraceLoggingRegistration& operator=(TraceLoggingRegistration&&) = delete;
};
}// namespace

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
static TraceLoggingRegistration gTraceLoggingRegistration;
#ifdef __clang__
#pragma clang diagnostic pop
#endif
