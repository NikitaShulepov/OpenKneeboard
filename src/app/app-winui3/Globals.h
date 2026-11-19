// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

#include <VDXROVR/DXResources.hpp>
#include <VDXROVR/RenderTargetID.hpp>

#include <VDXROVR/audited_ptr.hpp>

#include <winrt/VDXROVRApp.h>

#include <memory>
#include <vector>

namespace VDXROVR {
class KneeboardState;
class TroubleshootingStore;

extern HWND gMainWindow;
extern audited_weak_ptr<KneeboardState> gKneeboard;
extern audited_ptr<DXResources> gDXResources;
extern winrt::handle gMutex;
extern std::weak_ptr<TroubleshootingStore> gTroubleshootingStore;
extern std::vector<winrt::weak_ref<winrt::VDXROVRApp::TabPage>> gTabs;
extern RenderTargetID gGUIRenderTargetID;
extern bool gShuttingDown;

}// namespace VDXROVR
