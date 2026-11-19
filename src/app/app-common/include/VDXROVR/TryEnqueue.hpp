// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <VDXROVR/fatal.hpp>
#include <VDXROVR/scope_exit.hpp>

#include <shims/winrt/base.h>

#include <functional>
#include <type_traits>

namespace VDXROVR::detail {
template <class T>
concept awaitable = requires(T v) { static_cast<T&&>(v).operator co_await(); };
}// namespace VDXROVR::detail

namespace VDXROVR {

template <class TDQ, class TFn>
auto TryEnqueue(TDQ dq, TFn&& fn) {
  dq.TryEnqueue(
    std::bind_front(
      [](auto fn) -> VDXROVR::fire_and_forget {
        try {
          if constexpr (detail::awaitable<std::invoke_result_t<TFn>>) {
            co_await std::invoke(fn);
          } else {
            std::invoke(fn);
            co_return;
          }
        } catch (...) {
          fatal_with_exception(std::current_exception());
        }
      },
      std::forward<TFn>(fn)));
}

}// namespace VDXROVR
