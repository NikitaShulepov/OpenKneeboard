// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

// Helper macros when joining or stringifying other macros, e.g.:
//
// `FOO##__COUNTER__` becomes `FOO__COUNTER__`
// VDXROVR_CONCAT2(FOO, __COUNTER__) might become `FOO1`, for example

#define VDXROVR_CONCAT1(x, y) x##y
#define VDXROVR_CONCAT2(x, y) VDXROVR_CONCAT1(x, y)

#define VDXROVR_STRINGIFY1(x) #x
#define VDXROVR_STRINGIFY2(x) VDXROVR_STRINGIFY1(x)

// Helper for testing __VA_ARG__ behavior
#define VDXROVR_THIRD_ARG(a, b, c, ...) c

#define VDXROVR_VA_OPT_SUPPORTED_IMPL(...) \
  VDXROVR_THIRD_ARG(__VA_OPT__(, ), true, false, __VA_ARGS__)
#define VDXROVR_VA_OPT_SUPPORTED VDXROVR_VA_OPT_SUPPORTED_IMPL(JUNK)

#define VDXROVR_HAVE_NONSTANDARD_VA_ARGS_COMMA_ELISION_HELPER(X, ...) \
  X##__VA_ARGS__
#define VDXROVR_HAVE_NONSTANDARD_VA_ARGS_COMMA_ELISION \
  VDXROVR_THIRD_ARG( \
    VDXROVR_HAVE_NONSTANDARD_VA_ARGS_COMMA_ELISION_HELPER(JUNK), \
    false, \
    true)

#if VDXROVR_HAVE_NONSTANDARD_VA_ARGS_COMMA_ELISION
static_assert(
  VDXROVR_HAVE_NONSTANDARD_VA_ARGS_COMMA_ELISION_HELPER(123) == 123);
#endif

// https://developercommunity.visualstudio.com/t/std::unreachable-causes-warning-4702-un/10720390
#if defined(_MSC_VER) && !defined(__clang__)
#define VDXROVR_UNREACHABLE __assume(0)
#else
#define VDXROVR_UNREACHABLE std::unreachable();
#endif
