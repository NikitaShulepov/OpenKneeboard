// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <VDXROVR/IPageSourceWithDeveloperTools.hpp>
#include <VDXROVR/ITab.hpp>
#include <VDXROVR/KneeboardState.hpp>
#include <VDXROVR/TabDeveloperToolsAction.hpp>
#include <VDXROVR/TabView.hpp>

namespace VDXROVR {

TabDeveloperToolsAction::TabDeveloperToolsAction(
  KneeboardState* kneeboard,
  KneeboardViewID kneeboardView,
  const std::shared_ptr<TabView>& tabView)
  : ToolbarAction("\uEC7A", _("Developer tools")),
    mKneeboard(kneeboard),
    mKneeboardView(kneeboardView),
    mTabView(tabView) {
  AddEventListener(tabView->evPageChangedEvent, this->evStateChangedEvent);
}

TabDeveloperToolsAction::~TabDeveloperToolsAction() {
  this->RemoveAllEventListeners();
}

bool TabDeveloperToolsAction::IsEnabled() const {
  auto tv = mTabView.lock();
  if (!tv) {
    return false;
  }

  auto tab = tv->GetRootTab().lock();
  if (!tab) {
    return false;
  }
  auto inner = std::dynamic_pointer_cast<IPageSourceWithDeveloperTools>(tab);
  if (!inner) {
    return false;
  }
  return inner->HasDeveloperTools(tv->GetPageID());
}

task<void> TabDeveloperToolsAction::Execute() {
  auto tv = mTabView.lock();
  if (!tv) {
    co_return;
  }

  auto tab = tv->GetRootTab().lock();
  if (!tab) {
    co_return;
  }
  auto page = std::dynamic_pointer_cast<IPageSourceWithDeveloperTools>(tab);
  if (!page) {
    co_return;
  }
  page->OpenDeveloperToolsWindow(mKneeboardView, tv->GetPageID());
}

}// namespace VDXROVR
