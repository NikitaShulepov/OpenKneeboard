// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <VDXROVR/ToolbarAction.hpp>
#include <VDXROVR/UserActionHandler.hpp>

namespace VDXROVR {

class TabView;

class TabDeveloperToolsAction final : public ToolbarAction,
                                      private EventReceiver,
                                      public UserActionHandler {
 public:
  TabDeveloperToolsAction(
    KneeboardState*,
    KneeboardViewID kneeboardView,
    const std::shared_ptr<TabView>& state);
  TabDeveloperToolsAction() = delete;

  ~TabDeveloperToolsAction();

  virtual bool IsEnabled() const override;
  [[nodiscard]]
  virtual task<void> Execute() override;

 private:
  KneeboardState* mKneeboard = nullptr;
  KneeboardViewID mKneeboardView;
  std::weak_ptr<TabView> mTabView;
};

}// namespace VDXROVR
