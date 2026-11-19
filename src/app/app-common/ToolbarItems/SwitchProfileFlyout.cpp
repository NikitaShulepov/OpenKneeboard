// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <VDXROVR/ITab.hpp>
#include <VDXROVR/KneeboardState.hpp>
#include <VDXROVR/KneeboardView.hpp>
#include <VDXROVR/SwitchProfileAction.hpp>
#include <VDXROVR/SwitchProfileFlyout.hpp>
#include <VDXROVR/TabView.hpp>
#include <VDXROVR/TabsList.hpp>

namespace VDXROVR {

SwitchProfileFlyout::SwitchProfileFlyout(KneeboardState* kbs)
  : mKneeboardState(kbs) {
  this->AddEventListener(
    mKneeboardState->evProfileSettingsChangedEvent, this->evStateChangedEvent);
}

SwitchProfileFlyout::~SwitchProfileFlyout() { this->RemoveAllEventListeners(); }

std::string_view SwitchProfileFlyout::GetGlyph() const { return {}; }
std::string_view SwitchProfileFlyout::GetLabel() const {
  return _("Switch profile");
}

bool SwitchProfileFlyout::IsEnabled() const { return true; }

bool SwitchProfileFlyout::IsVisible() const {
  return mKneeboardState->GetProfileSettings().mEnabled;
}

std::vector<std::shared_ptr<IToolbarItem>> SwitchProfileFlyout::GetSubItems()
  const {
  std::vector<std::shared_ptr<IToolbarItem>> ret;
  for (const auto& profile:
       mKneeboardState->GetProfileSettings().GetSortedProfiles()) {
    ret.push_back(
      std::make_shared<SwitchProfileAction>(
        mKneeboardState, profile.mGuid, profile.mName));
  }
  return ret;
}

}// namespace VDXROVR
