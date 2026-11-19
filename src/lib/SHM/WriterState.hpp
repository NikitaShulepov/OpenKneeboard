// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <VDXROVR/StateMachine.hpp>

#include <VDXROVR/array.hpp>

namespace VDXROVR::SHM {

#define VDXROVR_SHM_WRITER_STATES \
  IT(Unlocked) \
  IT(TryLock) \
  IT(Locked) \
  IT(FrameInProgress) \
  IT(SubmittingFrame) \
  IT(SubmittingEmptyFrame) \
  IT(Detaching)

enum class WriterState {
#define IT(x) x,
  VDXROVR_SHM_WRITER_STATES
#undef IT
};

using WriterStateMachine = StateMachine<
  WriterState,
  WriterState::Unlocked,
  array_cat(
    lockable_transitions<WriterState>(),
    std::array {
      Transition {WriterState::Locked, WriterState::FrameInProgress},
      Transition {WriterState::FrameInProgress, WriterState::SubmittingFrame},
      Transition {WriterState::SubmittingFrame, WriterState::Locked},
      Transition {WriterState::Locked, WriterState::SubmittingEmptyFrame},
      Transition {WriterState::SubmittingEmptyFrame, WriterState::Locked},
      Transition {WriterState::Locked, WriterState::Detaching},
      Transition {WriterState::Detaching, WriterState::Locked},
    }),
  WriterState::Unlocked>;

static_assert(lockable_state_machine<SHM::WriterStateMachine>);

}// namespace VDXROVR::SHM
