// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <VDXROVR/ITab.hpp>
#include <VDXROVR/KneeboardState.hpp>
#include <VDXROVR/KneeboardView.hpp>
#include <VDXROVR/SwitchTabAction.hpp>
#include <VDXROVR/SwitchTabFlyout.hpp>
#include <VDXROVR/TabView.hpp>
#include <VDXROVR/TabsList.hpp>

namespace VDXROVR {

SwitchTabFlyout::SwitchTabFlyout(
  KneeboardState* kbs,
  const std::shared_ptr<KneeboardView>& kneeboardView)
  : mKneeboardState(kbs),
    mKneeboardView(kneeboardView) {
  AddEventListener(
    kbs->GetTabsList()->evTabsChangedEvent, this->evStateChangedEvent);
}

SwitchTabFlyout::~SwitchTabFlyout() { RemoveAllEventListeners(); }

std::string_view SwitchTabFlyout::GetGlyph() const { return {}; }
std::string_view SwitchTabFlyout::GetLabel() const { return _("Switch tab"); }

bool SwitchTabFlyout::IsEnabled() const { return true; }

std::vector<std::shared_ptr<IToolbarItem>> SwitchTabFlyout::GetSubItems()
  const {
  std::vector<std::shared_ptr<IToolbarItem>> ret;
  auto kbv = mKneeboardView.lock();
  if (!kbv) {
    return ret;
  }

  for (const auto& tab: mKneeboardState->GetTabsList()->GetTabs()) {
    ret.push_back(std::make_shared<SwitchTabAction>(kbv, tab));
  }
  return ret;
}

}// namespace VDXROVR
