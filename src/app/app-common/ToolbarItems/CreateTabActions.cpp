// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <VDXROVR/ClearUserInputAction.hpp>
#include <VDXROVR/CreateTabActions.hpp>
#include <VDXROVR/KneeboardState.hpp>
#include <VDXROVR/NextTabAction.hpp>
#include <VDXROVR/PreviousTabAction.hpp>
#include <VDXROVR/ReloadTabAction.hpp>
#include <VDXROVR/SwitchProfileFlyout.hpp>
#include <VDXROVR/SwitchTabFlyout.hpp>
#include <VDXROVR/TabDeveloperToolsAction.hpp>
#include <VDXROVR/TabFirstPageAction.hpp>
#include <VDXROVR/TabNavigationAction.hpp>
#include <VDXROVR/TabNextPageAction.hpp>
#include <VDXROVR/TabPreviousPageAction.hpp>
#include <VDXROVR/TabView.hpp>
#include <VDXROVR/ToggleBookmarkAction.hpp>
#include <VDXROVR/ToolbarAction.hpp>
#include <VDXROVR/ToolbarFlyout.hpp>
#include <VDXROVR/ToolbarSeparator.hpp>

#include <ranges>

namespace VDXROVR {

namespace {
using ItemPtr = std::shared_ptr<IToolbarItem>;
using Items = std::vector<ItemPtr>;
}// namespace

static ItemPtr CreateClearNotesItem(
  KneeboardState* kbs,
  const std::shared_ptr<KneeboardView>&,
  const std::shared_ptr<TabView>& tabView) {
  return std::make_shared<ToolbarFlyout>(
    "\ued60",// StrokeErase
    "Clear notes",
    Items {
      std::make_shared<ClearUserInputAction>(kbs, tabView, CurrentPage),
      std::make_shared<ClearUserInputAction>(kbs, tabView, AllPages),
      std::make_shared<ClearUserInputAction>(kbs, AllTabs),
    });
}

static ItemPtr CreateReloadItem(
  KneeboardState* kbs,
  const std::shared_ptr<KneeboardView>&,
  const std::shared_ptr<TabView>& tabView) {
  return std::make_shared<ToolbarFlyout>(
    "\ue72c",// Refresh
    "Reload",
    Items {
      std::make_shared<ReloadTabAction>(kbs, tabView),
      std::make_shared<ReloadTabAction>(kbs, AllTabs),
    });
}

InGameActions InGameActions::Create(
  KneeboardState* kneeboardState,
  const std::shared_ptr<KneeboardView>& kneeboardView,
  const std::shared_ptr<TabView>& tabView) {
  return {
    .mLeft =
      {
        std::make_shared<TabNavigationAction>(tabView),
        std::make_shared<TabFirstPageAction>(tabView),
        std::make_shared<TabPreviousPageAction>(kneeboardState, tabView),
        std::make_shared<TabNextPageAction>(kneeboardState, tabView),
      },
    .mRight =
      {
        std::make_shared<ToolbarFlyout>(
          "\ue712",
          _("More"),
          Items {
            std::make_shared<SwitchProfileFlyout>(kneeboardState),
            std::make_shared<SwitchTabFlyout>(kneeboardState, kneeboardView),
            std::make_shared<ToolbarSeparator>(),
            CreateClearNotesItem(kneeboardState, kneeboardView, tabView),
            CreateReloadItem(kneeboardState, kneeboardView, tabView),
          }),
        std::make_shared<ToggleBookmarkAction>(
          kneeboardState, kneeboardView, tabView),
        std::make_shared<PreviousTabAction>(kneeboardState, kneeboardView),
        std::make_shared<NextTabAction>(kneeboardState, kneeboardView),
      },
  };
}

InAppActions InAppActions::Create(
  KneeboardState* kneeboardState,
  const std::shared_ptr<KneeboardView>& kneeboardView,
  const std::shared_ptr<TabView>& tabView) {
  return {
    .mPrimary =
      {
        std::make_shared<TabNavigationAction>(tabView),
        std::make_shared<TabFirstPageAction>(tabView),
        std::make_shared<TabPreviousPageAction>(kneeboardState, tabView),
        std::make_shared<TabNextPageAction>(kneeboardState, tabView),
        std::make_shared<ToggleBookmarkAction>(
          kneeboardState, kneeboardView, tabView),
      },
    .mSecondary =
      {
        CreateClearNotesItem(kneeboardState, kneeboardView, tabView),
        CreateReloadItem(kneeboardState, kneeboardView, tabView),
        std::make_shared<ToolbarSeparator>(),
        std::make_shared<TabDeveloperToolsAction>(
          kneeboardState, kneeboardView->GetRuntimeID(), tabView),
      },
  };
}

}// namespace VDXROVR
