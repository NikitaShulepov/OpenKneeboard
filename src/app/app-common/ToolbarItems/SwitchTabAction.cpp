// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <VDXROVR/ITab.hpp>
#include <VDXROVR/KneeboardView.hpp>
#include <VDXROVR/SwitchTabAction.hpp>
#include <VDXROVR/TabView.hpp>
#include <VDXROVR/TabsList.hpp>

namespace VDXROVR {

SwitchTabAction::SwitchTabAction(
  const std::shared_ptr<KneeboardView>& kneeboardView,
  const std::shared_ptr<ITab>& tab)
  : ToolbarAction({}, tab->GetTitle()),
    mKneeboardView(kneeboardView),
    mTabID(tab->GetRuntimeID()) {
  AddEventListener(
    kneeboardView->evCurrentTabChangedEvent, this->evStateChangedEvent);
}

SwitchTabAction::~SwitchTabAction() { this->RemoveAllEventListeners(); }

bool SwitchTabAction::IsChecked() const {
  auto kbv = mKneeboardView.lock();
  if (!kbv) {
    return false;
  }
  const auto tab = kbv->GetCurrentTabView()->GetRootTab().lock();
  return tab && tab->GetRuntimeID() == mTabID;
}

bool SwitchTabAction::IsEnabled() const { return true; }

task<void> SwitchTabAction::Execute() {
  if (auto kbv = mKneeboardView.lock()) {
    kbv->SetCurrentTabByRuntimeID(mTabID);
  }
  co_return;
}

}// namespace VDXROVR
