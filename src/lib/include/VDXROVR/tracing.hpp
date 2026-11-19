// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <VDXROVR/macros.hpp>
#include <VDXROVR/scope_exit.hpp>

#include <Windows.h>
#include <winmeta.h>

#include <exception>
#include <source_location>

#include <TraceLoggingActivity.h>
#include <TraceLoggingProvider.h>

namespace VDXROVR {

/// Bitmask; we can use up to 48 bits here. Upper 16 bits are reserved for
/// Microsoft
enum class TraceLoggingEventKeywords {
  Uncategorized = 1,
  DPrint = 2,
  Events = 4,
  TaskCoro = 8,
};

wchar_t* GetFullPathForCurrentExecutable();

#define TraceLoggingThisExecutable() \
  TraceLoggingValue( \
    ::VDXROVR::GetFullPathForCurrentExecutable(), "Executable")

TRACELOGGING_DECLARE_PROVIDER(gTraceProvider);

#define VDXROVR_TraceLoggingSourceLocation(loc) \
  TraceLoggingValue(loc.file_name(), "File"), \
    TraceLoggingValue(loc.line(), "Line"), \
    TraceLoggingValue(loc.function_name(), "Function")

#define VDXROVR_TraceLoggingSize2D(size2d, name) \
  TraceLoggingValue(size2d.Width(), name "/Width"), \
    TraceLoggingValue(size2d.Height(), name "/Height")

#define VDXROVR_TraceLoggingRect(pr, name) \
  TraceLoggingValue(pr.Left(), name "/Left"), \
    TraceLoggingValue(pr.Top(), name "/Top"), \
    VDXROVR_TraceLoggingSize2D(pr, name)

using UncategorizedTraceLoggingThreadActivity = TraceLoggingThreadActivity<
  gTraceProvider,
  std::to_underlying(TraceLoggingEventKeywords::Uncategorized),
  WINEVENT_LEVEL_INFO>;

/** Create and automatically start and stop a named activity.
 *
 * @param VDXROVRTL_ACTIVITY the local variable to store the activity in
 * @param VDXROVRTL_NAME the name of the activity (C string literal)
 *
 * @see VDXROVR_TraceLoggingScope if you don't need the local variable
 *
 * This avoids templates and `auto` and generally jumps through hoops so that it
 * is valid both inside an implementation, and in a class definition.
 */
#define VDXROVR_TraceLoggingScopedActivity( \
  VDXROVRTL_ACTIVITY, VDXROVRTL_NAME, ...) \
  const std::function<void( \
    VDXROVR::UncategorizedTraceLoggingThreadActivity&)> \
    VDXROVR_CONCAT2(_StartImpl, VDXROVRTL_ACTIVITY) = \
      [&, loc = std::source_location::current()]( \
        VDXROVR::UncategorizedTraceLoggingThreadActivity& activity) { \
        TraceLoggingWriteStart( \
          activity, \
          VDXROVRTL_NAME, \
          VDXROVR_TraceLoggingSourceLocation(loc), \
          ##__VA_ARGS__); \
      }; \
  class VDXROVR_CONCAT2(_Impl, VDXROVRTL_ACTIVITY) final \
    : public VDXROVR::UncategorizedTraceLoggingThreadActivity { \
   public: \
    VDXROVR_CONCAT2(_Impl, VDXROVRTL_ACTIVITY) \
    (decltype(VDXROVR_CONCAT2(_StartImpl, VDXROVRTL_ACTIVITY))& startImpl) { \
      startImpl(*this); \
    } \
    VDXROVR_CONCAT2(~_Impl, VDXROVRTL_ACTIVITY)() { \
      if (mAutoStop) { \
        this->Stop(); \
      } \
    } \
    void Stop() { \
      if (mStopped) [[unlikely]] { \
        OutputDebugStringW(L"Double-stopped in Stop()"); \
        VDXROVR_BREAK; \
        return; \
      } \
      mStopped = true; \
      mAutoStop = false; \
      const auto exceptionCount = std::uncaught_exceptions(); \
      if (exceptionCount) [[unlikely]] { \
        TraceLoggingWriteStop( \
          *this, \
          VDXROVRTL_NAME, \
          TraceLoggingValue(exceptionCount, "UncaughtExceptions")); \
      } else { \
        TraceLoggingWriteStop(*this, VDXROVRTL_NAME); \
      } \
    } \
    void CancelAutoStop() { mAutoStop = false; } \
    _VDXROVR_TRACELOGGING_IMPL_StopWithResult(VDXROVRTL_NAME, int); \
    _VDXROVR_TRACELOGGING_IMPL_StopWithResult(VDXROVRTL_NAME, const char*); \
\
   private: \
    bool mStopped {false}; \
    bool mAutoStop {true}; \
  }; \
  VDXROVR_CONCAT2(_Impl, VDXROVRTL_ACTIVITY) \
  VDXROVRTL_ACTIVITY {VDXROVR_CONCAT2(_StartImpl, VDXROVRTL_ACTIVITY)};

// Not using templates as they're not permitted in local classes
#define _VDXROVR_TRACELOGGING_IMPL_StopWithResult( \
  VDXROVRTL_NAME, VDXROVRTL_RESULT_TYPE) \
  void StopWithResult(VDXROVRTL_RESULT_TYPE result) { \
    if (mStopped) [[unlikely]] { \
      OutputDebugStringW(L"Double-stopped in StopWithResult()"); \
      VDXROVR_BREAK; \
      return; \
    } \
    this->CancelAutoStop(); \
    mStopped = true; \
    TraceLoggingWriteStop( \
      *this, VDXROVRTL_NAME, TraceLoggingValue(result, "Result")); \
  }

/** Create and automatically start and stop a named activity.
 *
 * Convenience wrapper around VDXROVR_TraceLoggingScopedActivity
 * that generates the local variable names.
 *
 * @param VDXROVRTL_NAME the name of the activity (C string literal)
 */
#define VDXROVR_TraceLoggingScope(VDXROVRTL_NAME, ...) \
  VDXROVR_TraceLoggingScopedActivity( \
    VDXROVR_CONCAT2(_vdxrovrtlsa, __COUNTER__), VDXROVRTL_NAME, ##__VA_ARGS__)

#define VDXROVR_TraceLoggingWrite(VDXROVRTL_NAME, ...) \
  TraceLoggingWrite( \
    gTraceProvider, \
    VDXROVRTL_NAME, \
    TraceLoggingLevel(WINEVENT_LEVEL_INFO), \
    TraceLoggingKeyword( \
      std::to_underlying(TraceLoggingEventKeywords::Uncategorized)), \
    TraceLoggingValue(__FILE__, "File"), \
    TraceLoggingValue(__LINE__, "Line"), \
    TraceLoggingValue(__FUNCTION__, "Function"), \
    ##__VA_ARGS__)

#define VDXROVR_TraceLoggingCoro(VDXROVRTL_NAME, ...) \
  VDXROVR_TraceLoggingWrite( \
    VDXROVRTL_NAME, TraceLoggingOpcode(WINEVENT_OPCODE_START), ##__VA_ARGS__); \
  const ::VDXROVR::scope_exit VDXROVR_CONCAT2( \
    _vdxrovrtllcoro__, __COUNTER__) {[&, n = std::uncaught_exceptions()]() { \
    VDXROVR_TraceLoggingWrite( \
      VDXROVRTL_NAME, \
      TraceLoggingOpcode(WINEVENT_OPCODE_STOP), \
      TraceLoggingValue(std::uncaught_exceptions() - n, "Exceptions"), \
      ##__VA_ARGS__); \
  }};

#define VDXROVR_TraceLoggingStringView(value, name) \
  TraceLoggingCountedUtf8String(value.data(), value.size(), name)

}// namespace VDXROVR
