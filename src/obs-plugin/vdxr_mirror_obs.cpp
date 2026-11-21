// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include "obs_abi_minimal.hpp"

#include <VDXROVR/VRMirrorSHM.hpp>
#include <VDXROVR/WASAPIProcessLoopback.hpp>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

using namespace VDXROVR;

namespace {

LibObsApi gObs {};
obs_module_t* gModulePointer {nullptr};

constexpr char kPropFollowApp[] = "follow_app_settings";
constexpr char kPropEyeMode[] = "eye_mode";
constexpr char kPropResolution[] = "resolution";
constexpr char kPropShowOverlays[] = "show_overlays";
constexpr char kPropCaptureGameAudio[] = "capture_game_audio";
constexpr char kPropSilenceWhenIdle[] = "silence_when_idle";

struct MirrorObsSource final {
  obs_source_t* mSource {nullptr};
  std::atomic<bool> mRunning {false};
  std::atomic<bool> mFollowAppSettings {true};
  std::atomic<int> mEyeMode {0};
  std::atomic<int> mResolution {0}; // 0=Auto, 1=720p, 2=1080p
  std::atomic<bool> mShowOverlays {true};
  std::atomic<bool> mCaptureGameAudio {true};
  std::atomic<bool> mSilenceWhenIdle {true};

  HANDLE mMappingHandle {nullptr};
  VRMirrorSharedBuffer* mSharedBuffer {nullptr};
  uint64_t mLastFrameSeq {0};
  uint32_t mCurrentWidth {1920};
  uint32_t mCurrentHeight {1080};
  uint32_t mActiveAudioPid {0};
  std::vector<uint8_t> mVideoFrameCopy;
  std::vector<float> mIdleSilence;
  WASAPIProcessLoopbackCapture mLoopback;
  std::thread mWorker;

  void EnsureSharedMemory() noexcept {
    if (mSharedBuffer) {
      return;
    }
    const uint64_t totalBytes = sizeof(VRMirrorSharedBuffer);
    mMappingHandle = CreateFileMappingW(
      INVALID_HANDLE_VALUE,
      nullptr,
      PAGE_READWRITE,
      static_cast<DWORD>(totalBytes >> 32),
      static_cast<DWORD>(totalBytes & 0xFFFFFFFFull),
      kVRMirrorSharedMemoryName);
    if (!mMappingHandle) {
      return;
    }
    const bool newlyCreated = (GetLastError() != ERROR_ALREADY_EXISTS);
    void* mapped = MapViewOfFile(
      mMappingHandle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(VRMirrorSharedBuffer));
    if (!mapped) {
      CloseHandle(mMappingHandle);
      mMappingHandle = nullptr;
      return;
    }
    mSharedBuffer = static_cast<VRMirrorSharedBuffer*>(mapped);
    if (
      newlyCreated
      || mSharedBuffer->mHeader.mMagic != kVRMirrorSharedMagic
      || mSharedBuffer->mHeader.mVersion != kVRMirrorSharedVersion) {
      std::memset(&mSharedBuffer->mHeader, 0, sizeof(VRMirrorSharedHeader));
      mSharedBuffer->mHeader.mMagic = kVRMirrorSharedMagic;
      mSharedBuffer->mHeader.mVersion = kVRMirrorSharedVersion;
      mSharedBuffer->mHeader.mPixelFormat = kVRMirrorPixelFormatBGRA8;
      mSharedBuffer->mHeader.mConsumerDesiredEyeMode = kVRMirrorUseAppSetting;
      mSharedBuffer->mHeader.mConsumerDesiredResolution = kVRMirrorUseAppSetting;
      mSharedBuffer->mHeader.mConsumerDesiredShowOverlays = kVRMirrorUseAppSetting;
    }
  }

  void ApplyConsumerOverrides() noexcept {
    if (!mSharedBuffer) {
      return;
    }
    auto& hdr = mSharedBuffer->mHeader;
    if (mFollowAppSettings.load(std::memory_order_acquire)) {
      VRMirrorWriteU32Atomic(&hdr.mConsumerDesiredEyeMode, kVRMirrorUseAppSetting);
      VRMirrorWriteU32Atomic(&hdr.mConsumerDesiredResolution, kVRMirrorUseAppSetting);
      VRMirrorWriteU32Atomic(&hdr.mConsumerDesiredShowOverlays, kVRMirrorUseAppSetting);
    } else {
      const int eye = mEyeMode.load(std::memory_order_acquire);
      const int res = mResolution.load(std::memory_order_acquire);
      const bool ovl = mShowOverlays.load(std::memory_order_acquire);
      VRMirrorWriteU32Atomic(
        &hdr.mConsumerDesiredEyeMode,
        static_cast<uint32_t>(eye >= 0 && eye <= 2 ? eye : 0));
      VRMirrorWriteU32Atomic(
        &hdr.mConsumerDesiredResolution,
        res == 1 ? 0u : (res == 2 ? 1u : kVRMirrorUseAppSetting));
      VRMirrorWriteU32Atomic(&hdr.mConsumerDesiredShowOverlays, ovl ? 1u : 0u);
    }
  }

  void SyncAudio(const uint32_t gamePid, const bool appCaptureAudio) {
    const bool follow = mFollowAppSettings.load(std::memory_order_acquire);
    const bool wantAudio = mCaptureGameAudio.load(std::memory_order_acquire)
      && (!follow || appCaptureAudio)
      && (gamePid != 0u);

    if (!wantAudio) {
      if (mActiveAudioPid != 0u) {
        mLoopback.Stop();
        mActiveAudioPid = 0u;
      }
      return;
    }

    if (mActiveAudioPid == gamePid && mLoopback.IsActive()) {
      return;
    }

    mLoopback.Stop();
    if (mLoopback.Start(
          gamePid,
          [this](
            const float* samples,
            const uint32_t frames,
            const uint32_t sampleRate,
            const uint64_t) {
            if (!mRunning.load(std::memory_order_acquire) || !mSource || !samples || frames == 0u) {
              return;
            }
            obs_source_audio audio {};
            audio.data[0] = reinterpret_cast<const uint8_t*>(samples);
            audio.frames = frames;
            audio.speakers = SPEAKERS_STEREO;
            audio.format = AUDIO_FORMAT_FLOAT;
            audio.samples_per_sec = sampleRate;
            audio.timestamp = gObs.os_gettime_ns();
            gObs.obs_source_output_audio(mSource, &audio);
          })) {
      mActiveAudioPid = gamePid;
    }
  }

  void EmitIdleSilenceIfNeeded() {
    if (!mSilenceWhenIdle.load(std::memory_order_acquire) || !mSource) {
      return;
    }
    constexpr uint32_t kSilenceFrames = 480u; // 10 ms at 48 kHz
    if (mIdleSilence.size() != static_cast<size_t>(kSilenceFrames) * 2u) {
      mIdleSilence.assign(static_cast<size_t>(kSilenceFrames) * 2u, 0.0f);
    }
    obs_source_audio audio {};
    audio.data[0] = reinterpret_cast<const uint8_t*>(mIdleSilence.data());
    audio.frames = kSilenceFrames;
    audio.speakers = SPEAKERS_STEREO;
    audio.format = AUDIO_FORMAT_FLOAT;
    audio.samples_per_sec = WASAPIProcessLoopbackCapture::kSampleRate;
    audio.timestamp = gObs.os_gettime_ns();
    gObs.obs_source_output_audio(mSource, &audio);
  }

  void WorkerLoop() {
    uint64_t lastSilenceMs = 0u;
    while (mRunning.load(std::memory_order_acquire)) {
      this->EnsureSharedMemory();
      const uint64_t nowMs = GetTickCount64();
      bool activeGame = false;

      if (mSharedBuffer) {
        VRMirrorWriteU64Atomic(
          &mSharedBuffer->mHeader.mObsConsumerHeartbeatMs, nowMs);
        this->ApplyConsumerOverrides();

        const bool prodActive =
          IsVRMirrorProducerActive(mSharedBuffer->mHeader, nowMs);
        const uint32_t gamePid = prodActive
          ? VRMirrorReadU32Atomic(&mSharedBuffer->mHeader.mGameProcessId)
          : 0u;
        const bool appAudio = (mSharedBuffer->mHeader.mCaptureGameAudio != 0u);
        this->SyncAudio(gamePid, appAudio);

        if (prodActive && mSharedBuffer->mHeader.mHasPixelData != 0u) {
          activeGame = true;
          const uint64_t seq =
            VRMirrorReadU64Atomic(&mSharedBuffer->mHeader.mFrameSequence);
          if (seq != mLastFrameSeq) {
            const uint32_t slot =
              VRMirrorReadU32Atomic(&mSharedBuffer->mHeader.mActiveSlot) & 1u;
            const uint32_t fw = mSharedBuffer->mHeader.mWidth;
            const uint32_t fh = mSharedBuffer->mHeader.mHeight;
            const size_t bytes = static_cast<size_t>(fw) * static_cast<size_t>(fh) * 4u;
            if (
              fw > 0u && fh > 0u && fw <= kVRMirrorMaxWidth
              && fh <= kVRMirrorMaxHeight && bytes <= kVRMirrorSlotBytes) {
              std::atomic_thread_fence(std::memory_order_acquire);
              mVideoFrameCopy.resize(bytes);
              std::memcpy(mVideoFrameCopy.data(), mSharedBuffer->mSlots[slot], bytes);
              mCurrentWidth = fw;
              mCurrentHeight = fh;
              mLastFrameSeq = seq;

              obs_source_frame frame {};
              frame.data[0] = mVideoFrameCopy.data();
              frame.linesize[0] = fw * 4u;
              frame.width = fw;
              frame.height = fh;
              frame.timestamp = gObs.os_gettime_ns();
              frame.format = VIDEO_FORMAT_BGRA;
              frame.full_range = true;
              gObs.obs_source_output_video(mSource, &frame);
            }
          }
        }
      }

      if (!activeGame) {
        if (mLastFrameSeq != 0u) {
          // Clear stale frame cleanly when VR game exits
          gObs.obs_source_output_video(mSource, nullptr);
          mLastFrameSeq = 0u;
        }
        if (nowMs - lastSilenceMs >= 10u) {
          this->EmitIdleSilenceIfNeeded();
          lastSilenceMs = nowMs;
        }
        Sleep(10u);
      } else {
        Sleep(4u);
      }
    }

    mLoopback.Stop();
    if (mSharedBuffer) {
      VRMirrorWriteU64Atomic(&mSharedBuffer->mHeader.mObsConsumerHeartbeatMs, 0u);
      VRMirrorWriteU32Atomic(
        &mSharedBuffer->mHeader.mConsumerDesiredEyeMode, kVRMirrorUseAppSetting);
      VRMirrorWriteU32Atomic(
        &mSharedBuffer->mHeader.mConsumerDesiredResolution, kVRMirrorUseAppSetting);
      VRMirrorWriteU32Atomic(
        &mSharedBuffer->mHeader.mConsumerDesiredShowOverlays, kVRMirrorUseAppSetting);
      UnmapViewOfFile(mSharedBuffer);
      mSharedBuffer = nullptr;
    }
    if (mMappingHandle) {
      CloseHandle(mMappingHandle);
      mMappingHandle = nullptr;
    }
  }
};

const char* SourceGetName(void*) {
  return "VDXR Mirror (Video + Game Audio)";
}

void SourceGetDefaults(obs_data_t* settings) {
  gObs.obs_data_set_default_bool(settings, kPropFollowApp, true);
  gObs.obs_data_set_default_int(settings, kPropEyeMode, 0);
  gObs.obs_data_set_default_int(settings, kPropResolution, 0);
  gObs.obs_data_set_default_bool(settings, kPropShowOverlays, true);
  gObs.obs_data_set_default_bool(settings, kPropCaptureGameAudio, true);
  gObs.obs_data_set_default_bool(settings, kPropSilenceWhenIdle, true);
}

obs_properties_t* SourceGetProperties(void*) {
  obs_properties_t* props = gObs.obs_properties_create();
  gObs.obs_properties_add_bool(
    props,
    kPropFollowApp,
    "Follow VDXROVR settings");

  obs_property_t* eyeList = gObs.obs_properties_add_list(
    props,
    kPropEyeMode,
    "Eye selection (override)",
    OBS_COMBO_TYPE_LIST,
    OBS_COMBO_FORMAT_INT);
  gObs.obs_property_list_add_int(eyeList, "Left Eye", 0);
  gObs.obs_property_list_add_int(eyeList, "Right Eye", 1);
  gObs.obs_property_list_add_int(eyeList, "Side-by-Side (16:9)", 2);

  obs_property_t* resList = gObs.obs_properties_add_list(
    props,
    kPropResolution,
    "Output resolution (override)",
    OBS_COMBO_TYPE_LIST,
    OBS_COMBO_FORMAT_INT);
  gObs.obs_property_list_add_int(resList, "Auto", 0);
  gObs.obs_property_list_add_int(resList, "1280x720 (720p)", 1);
  gObs.obs_property_list_add_int(resList, "1920x1080 (1080p)", 2);

  gObs.obs_properties_add_bool(
    props,
    kPropShowOverlays,
    "Show VR overlays (override)");
  gObs.obs_properties_add_bool(
    props,
    kPropCaptureGameAudio,
    "Capture VR game audio (WASAPI Process Loopback)");
  gObs.obs_properties_add_bool(
    props,
    kPropSilenceWhenIdle,
    "Output clean silence while waiting for VR game");
  return props;
}

void SourceUpdate(void* data, obs_data_t* settings) {
  auto* ctx = static_cast<MirrorObsSource*>(data);
  if (!ctx || !settings) {
    return;
  }
  ctx->mFollowAppSettings.store(
    gObs.obs_data_get_bool(settings, kPropFollowApp), std::memory_order_release);
  ctx->mEyeMode.store(
    static_cast<int>(gObs.obs_data_get_int(settings, kPropEyeMode)),
    std::memory_order_release);
  ctx->mResolution.store(
    static_cast<int>(gObs.obs_data_get_int(settings, kPropResolution)),
    std::memory_order_release);
  ctx->mShowOverlays.store(
    gObs.obs_data_get_bool(settings, kPropShowOverlays), std::memory_order_release);
  ctx->mCaptureGameAudio.store(
    gObs.obs_data_get_bool(settings, kPropCaptureGameAudio),
    std::memory_order_release);
  ctx->mSilenceWhenIdle.store(
    gObs.obs_data_get_bool(settings, kPropSilenceWhenIdle),
    std::memory_order_release);
}

void* SourceCreate(obs_data_t* settings, obs_source_t* source) {
  auto* ctx = new MirrorObsSource();
  ctx->mSource = source;
  SourceUpdate(ctx, settings);
  ctx->mRunning.store(true, std::memory_order_release);
  ctx->mWorker = std::thread([ctx]() { ctx->WorkerLoop(); });
  return ctx;
}

void SourceDestroy(void* data) {
  auto* ctx = static_cast<MirrorObsSource*>(data);
  if (!ctx) {
    return;
  }
  ctx->mRunning.store(false, std::memory_order_release);
  if (ctx->mWorker.joinable()) {
    ctx->mWorker.join();
  }
  delete ctx;
}

uint32_t SourceGetWidth(void* data) {
  const auto* ctx = static_cast<const MirrorObsSource*>(data);
  return ctx ? ctx->mCurrentWidth : 1920u;
}

uint32_t SourceGetHeight(void* data) {
  const auto* ctx = static_cast<const MirrorObsSource*>(data);
  return ctx ? ctx->mCurrentHeight : 1080u;
}

}// namespace

extern "C" {

__declspec(dllexport) void obs_module_set_pointer(obs_module_t* module) {
  gModulePointer = module;
}

__declspec(dllexport) uint32_t obs_module_ver(void) {
  return OK_LIBOBS_API_VER;
}

__declspec(dllexport) const char* obs_module_name(void) {
  return "VDXROVR VDXR Mirror";
}

__declspec(dllexport) const char* obs_module_description(void) {
  return "Captures OpenXR VR eye projection (16:9) with optional overlays and isolated VR game audio via WASAPI Process Loopback.";
}

__declspec(dllexport) bool obs_module_load(void) {
  if (!gObs.Load()) {
    return false;
  }
  obs_source_info info {};
  info.id = "vdxrovr_vdxr_mirror_source";
  info.type = OBS_SOURCE_TYPE_INPUT;
  info.output_flags =
    OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_AUDIO | OBS_SOURCE_DO_NOT_DUPLICATE;
  info.get_name = &SourceGetName;
  info.create = &SourceCreate;
  info.destroy = &SourceDestroy;
  info.get_width = &SourceGetWidth;
  info.get_height = &SourceGetHeight;
  info.get_defaults = &SourceGetDefaults;
  info.get_properties = &SourceGetProperties;
  info.update = &SourceUpdate;
  info.icon_type = OBS_ICON_TYPE_GAME_CAPTURE;
  gObs.obs_register_source_s(&info, sizeof(info));
  return true;
}

__declspec(dllexport) void obs_module_unload(void) {}

}// extern "C"
