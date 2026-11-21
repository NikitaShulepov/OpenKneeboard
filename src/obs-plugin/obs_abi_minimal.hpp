// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

// Minimal binary-stable libobs C ABI header for OBS Studio 28-31+ (Windows x64).
// All libobs functions are dynamically resolved from the host obs.dll at module
// load time so CI builds cleanly under MSVC C++20 /WX without downloading OBS.

#include <Windows.h>

#include <cstddef>
#include <cstdint>

extern "C" {

constexpr uint32_t OK_LIBOBS_API_VER = (28u << 24) | (0u << 16);
constexpr size_t OK_OBS_MAX_AV_PLANES = 8u;

struct obs_module_t;
struct obs_source_t;
struct obs_data_t;
struct obs_properties_t;
struct obs_property_t;

enum obs_source_type : int {
  OBS_SOURCE_TYPE_INPUT = 0,
  OBS_SOURCE_TYPE_FILTER = 1,
  OBS_SOURCE_TYPE_TRANSITION = 2,
  OBS_SOURCE_TYPE_SCENE = 3,
};

enum obs_icon_type : int {
  OBS_ICON_TYPE_UNKNOWN = 0,
  OBS_ICON_TYPE_GAME_CAPTURE = 9,
};

constexpr uint32_t OBS_SOURCE_VIDEO = (1u << 0);
constexpr uint32_t OBS_SOURCE_AUDIO = (1u << 1);
constexpr uint32_t OBS_SOURCE_ASYNC = (1u << 2);
constexpr uint32_t OBS_SOURCE_ASYNC_VIDEO = (OBS_SOURCE_ASYNC | OBS_SOURCE_VIDEO);
constexpr uint32_t OBS_SOURCE_DO_NOT_DUPLICATE = (1u << 7);

enum video_format : int {
  VIDEO_FORMAT_NONE = 0,
  VIDEO_FORMAT_I420 = 1,
  VIDEO_FORMAT_NV12 = 2,
  VIDEO_FORMAT_YVYU = 3,
  VIDEO_FORMAT_YUY2 = 4,
  VIDEO_FORMAT_UYVY = 5,
  VIDEO_FORMAT_RGBA = 6,
  VIDEO_FORMAT_BGRA = 7,
  VIDEO_FORMAT_BGRX = 8,
};

enum audio_format : int {
  AUDIO_FORMAT_UNKNOWN = 0,
  AUDIO_FORMAT_U8BIT = 1,
  AUDIO_FORMAT_16BIT = 2,
  AUDIO_FORMAT_32BIT = 3,
  AUDIO_FORMAT_FLOAT = 4,
};

enum speaker_layout : int {
  SPEAKERS_UNKNOWN = 0,
  SPEAKERS_MONO = 1,
  SPEAKERS_STEREO = 2,
};

enum obs_combo_type : int {
  OBS_COMBO_TYPE_INVALID = 0,
  OBS_COMBO_TYPE_EDITABLE = 1,
  OBS_COMBO_TYPE_LIST = 2,
  OBS_COMBO_TYPE_RADIO = 3,
};

enum obs_combo_format : int {
  OBS_COMBO_FORMAT_INVALID = 0,
  OBS_COMBO_FORMAT_INT = 1,
  OBS_COMBO_FORMAT_FLOAT = 2,
  OBS_COMBO_FORMAT_STRING = 3,
  OBS_COMBO_FORMAT_BOOL = 4,
};

struct obs_source_frame {
  uint8_t* data[OK_OBS_MAX_AV_PLANES];
  uint32_t linesize[OK_OBS_MAX_AV_PLANES];
  uint32_t width;
  uint32_t height;
  uint64_t timestamp;
  video_format format;
  float color_matrix[16];
  bool full_range;
  uint16_t max_luminance;
  float color_range_min[3];
  float color_range_max[3];
  bool flip;
  uint8_t flags;
  uint8_t trc;
  volatile long refs;
  bool prev_frame;
};

struct obs_source_audio {
  const uint8_t* data[OK_OBS_MAX_AV_PLANES];
  uint32_t frames;
  speaker_layout speakers;
  audio_format format;
  uint32_t samples_per_sec;
  uint64_t timestamp;
};

struct obs_source_info {
  const char* id;
  obs_source_type type;
  uint32_t output_flags;
  const char* (*get_name)(void* type_data);
  void* (*create)(obs_data_t* settings, obs_source_t* source);
  void (*destroy)(void* data);
  uint32_t (*get_width)(void* data);
  uint32_t (*get_height)(void* data);
  void (*get_defaults)(obs_data_t* settings);
  obs_properties_t* (*get_properties)(void* data);
  void (*update)(void* data, obs_data_t* settings);
  void (*activate)(void* data);
  void (*deactivate)(void* data);
  void (*show)(void* data);
  void (*hide)(void* data);
  void (*video_tick)(void* data, float seconds);
  void (*video_render)(void* data, void* effect);
  obs_source_frame* (*filter_video)(void* data, obs_source_frame* frame);
  void* (*filter_audio)(void* data, void* audio);
  void (*enum_active_sources)(void* data, void* enum_callback, void* param);
  void (*save)(void* data, obs_data_t* settings);
  void (*load)(void* data, obs_data_t* settings);
  void (*mouse_click)(void* data, const void* event, int32_t type, bool mouse_up, uint32_t click_count);
  void (*mouse_move)(void* data, const void* event, bool mouse_leave);
  void (*mouse_wheel)(void* data, const void* event, int x_delta, int y_delta);
  void (*focus)(void* data, bool focus);
  void (*key_click)(void* data, const void* event, bool key_up);
  void (*filter_remove)(void* data, obs_source_t* source);
  void* type_data;
  void (*free_type_data)(void* type_data);
  bool (*audio_render)(void* data, uint64_t* ts_out, void* audio_output, uint32_t mixers, size_t channels, size_t sample_rate);
  void (*enum_all_sources)(void* data, void* enum_callback, void* param);
  void (*transition_start)(void* data);
  void (*transition_stop)(void* data);
  void (*get_defaults2)(void* type_data, obs_data_t* settings);
  obs_properties_t* (*get_properties2)(void* data, void* type_data);
  bool (*audio_mix)(void* data, uint64_t* ts_out, void* audio_output, size_t channels, size_t sample_rate);
  obs_icon_type icon_type;
};

}// extern "C"

struct LibObsApi {
  void (*obs_register_source_s)(const obs_source_info* info, size_t size) {nullptr};
  void (*obs_source_output_video)(obs_source_t* source, const obs_source_frame* frame) {nullptr};
  void (*obs_source_output_audio)(obs_source_t* source, const obs_source_audio* audio) {nullptr};
  obs_properties_t* (*obs_properties_create)() {nullptr};
  obs_property_t* (*obs_properties_add_bool)(
    obs_properties_t* props,
    const char* name,
    const char* description) {nullptr};
  obs_property_t* (*obs_properties_add_list)(
    obs_properties_t* props,
    const char* name,
    const char* description,
    obs_combo_type type,
    obs_combo_format format) {nullptr};
  size_t (*obs_property_list_add_int)(
    obs_property_t* p,
    const char* name,
    long long val) {nullptr};
  void (*obs_data_set_default_bool)(
    obs_data_t* data,
    const char* name,
    bool val) {nullptr};
  void (*obs_data_set_default_int)(
    obs_data_t* data,
    const char* name,
    long long val) {nullptr};
  bool (*obs_data_get_bool)(obs_data_t* data, const char* name) {nullptr};
  long long (*obs_data_get_int)(obs_data_t* data, const char* name) {nullptr};
  uint64_t (*os_gettime_ns)() {nullptr};

  bool Load() noexcept {
    HMODULE mod = GetModuleHandleW(L"obs.dll");
    if (!mod) {
      return false;
    }
    auto resolve = [mod]<typename Fn>(Fn& out, const char* symbol) -> bool {
      out = reinterpret_cast<Fn>(GetProcAddress(mod, symbol));
      return out != nullptr;
    };
    return resolve(obs_register_source_s, "obs_register_source_s")
      && resolve(obs_source_output_video, "obs_source_output_video")
      && resolve(obs_source_output_audio, "obs_source_output_audio")
      && resolve(obs_properties_create, "obs_properties_create")
      && resolve(obs_properties_add_bool, "obs_properties_add_bool")
      && resolve(obs_properties_add_list, "obs_properties_add_list")
      && resolve(obs_property_list_add_int, "obs_property_list_add_int")
      && resolve(obs_data_set_default_bool, "obs_data_set_default_bool")
      && resolve(obs_data_set_default_int, "obs_data_set_default_int")
      && resolve(obs_data_get_bool, "obs_data_get_bool")
      && resolve(obs_data_get_int, "obs_data_get_int")
      && resolve(os_gettime_ns, "os_gettime_ns");
  }
};

