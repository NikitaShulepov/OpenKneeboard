// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <VDXROVR/Vulkan/Dispatch.hpp>
#include <VDXROVR/Vulkan/SpriteBatch.hpp>
#include <VDXROVR/Vulkan/vkresult.hpp>

#include <VDXROVR/dprint.hpp>

#include <shims/vulkan/vulkan.h>

namespace VDXROVR::Vulkan {

std::optional<uint32_t> FindMemoryType(
  Dispatch*,
  VkPhysicalDevice,
  uint32_t filter,
  VkMemoryPropertyFlags flags);
}// namespace VDXROVR::Vulkan
