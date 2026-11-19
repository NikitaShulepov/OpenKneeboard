// VDXROVR
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#pragma once

// clang-format off
#include "pch.h"
#include "TabsSettingsPage.g.h"

#include "TabUIData.g.h"
#include "BrowserTabUIData.g.h"
#include "DCSRadioLogTabUIData.g.h"
#include "WindowCaptureTabUIData.g.h"

#include "TabUIDataTemplateSelector.g.h"
// clang-format on

#include "WithPropertyChangedEvent.h"

#include <VDXROVR/Events.hpp>

#include <VDXROVR/audited_ptr.hpp>
#include <VDXROVR/task.hpp>

#include <string>

namespace VDXROVR {
class ITab;
class TabView;
class BrowserTab;
class DCSRadioLogTab;
struct DXResources;
class KneeboardState;
class WindowCaptureTab;
enum class TabType;
}// namespace VDXROVR

using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Controls::Primitives;
using namespace winrt::Windows::Foundation::Collections;

namespace winrt::VDXROVRApp::implementation {
struct TabsSettingsPage : TabsSettingsPageT<TabsSettingsPage>,
                          VDXROVR::EventReceiver,
                          VDXROVR::WithPropertyChangedEvent {
  TabsSettingsPage();
  ~TabsSettingsPage() noexcept override;

  IVector<IInspectable> Tabs() noexcept;

  VDXROVR::fire_and_forget RestoreDefaults(
    IInspectable,
    RoutedEventArgs) noexcept;

  VDXROVR::fire_and_forget CreateTab(
    IInspectable,
    RoutedEventArgs) noexcept;
  VDXROVR::fire_and_forget CreatePluginTab(
    IInspectable,
    RoutedEventArgs) noexcept;
  VDXROVR::fire_and_forget RemoveTab(IInspectable, RoutedEventArgs);
  VDXROVR::fire_and_forget ShowTabSettings(IInspectable, RoutedEventArgs);
  VDXROVR::fire_and_forget ShowDebugInfo(IInspectable, RoutedEventArgs);
  void CopyDebugInfo(const IInspectable&, const RoutedEventArgs&);

  VDXROVR::fire_and_forget OnTabsChanged(
    IInspectable,
    Windows::Foundation::Collections::IVectorChangedEventArgs) noexcept;
  void OnAddBrowserAddressTextChanged(
    const IInspectable&,
    const IInspectable&) noexcept;

 private:
  void CreateAddTabMenu(const Button& button, FlyoutPlacementMode);

  template <class T>
  VDXROVR::fire_and_forget CreateFileTab(
    const std::string& pickerDialogTitle = {});
  VDXROVR::fire_and_forget CreateFolderTab();
  VDXROVR::fire_and_forget CreateWindowCaptureTab();
  VDXROVR::fire_and_forget CreateBrowserTab();
  winrt::guid GetFilePickerPersistenceGuid();

  task<void> AddTabs(const std::vector<std::shared_ptr<VDXROVR::ITab>>&);
  static VDXROVRApp::TabUIData CreateTabUIData(
    const std::shared_ptr<VDXROVR::ITab>&);

  bool mUIIsChangingTabs = false;

  VDXROVR::audited_ptr<VDXROVR::DXResources> mDXR;
  VDXROVR::audited_ptr<VDXROVR::KneeboardState> mKneeboard;
};

struct TabUIData : TabUIDataT<TabUIData>,
                   VDXROVR::EventReceiver,
                   VDXROVR::WithPropertyChangedEvent {
  TabUIData() = default;
  ~TabUIData();

  uint64_t InstanceID() const;
  void InstanceID(uint64_t);

  hstring Title() const;
  void Title(hstring);

  bool HasDebugInformation() const;
  hstring DebugInformation() const;

 protected:
  std::weak_ptr<VDXROVR::ITab> mTab;
};

struct BrowserTabUIData : BrowserTabUIDataT<
                            BrowserTabUIData,
                            VDXROVRApp::implementation::TabUIData> {
  BrowserTabUIData() = default;

  bool IsSimHubIntegrationEnabled() const;
  VDXROVR::fire_and_forget IsSimHubIntegrationEnabled(bool);

  bool AreVDXROVRAPIsEnabled() const;
  VDXROVR::fire_and_forget AreVDXROVRAPIsEnabled(bool);

  bool IsBackgroundTransparent() const;
  VDXROVR::fire_and_forget IsBackgroundTransparent(bool);

 private:
  std::shared_ptr<VDXROVR::BrowserTab> GetTab() const;
};

struct DCSRadioLogTabUIData : DCSRadioLogTabUIDataT<
                                DCSRadioLogTabUIData,
                                VDXROVRApp::implementation::TabUIData> {
  DCSRadioLogTabUIData() = default;
  uint8_t MissionStartBehavior() const;
  void MissionStartBehavior(uint8_t value);
  bool TimestampsEnabled() const;
  void TimestampsEnabled(bool value);

 private:
  std::shared_ptr<VDXROVR::DCSRadioLogTab> GetTab() const;
};

struct WindowCaptureTabUIData : WindowCaptureTabUIDataT<
                                  WindowCaptureTabUIData,
                                  VDXROVRApp::implementation::TabUIData> {
  WindowCaptureTabUIData() = default;

  using fire_and_forget = VDXROVR::fire_and_forget;

  hstring WindowTitle();
  fire_and_forget WindowTitle(hstring const& value);
  bool MatchWindowClass();
  fire_and_forget MatchWindowClass(bool value);
  uint8_t MatchWindowTitle();
  fire_and_forget MatchWindowTitle(uint8_t value);
  bool IsInputEnabled() const;
  void IsInputEnabled(bool value);
  bool IsCursorCaptureEnabled() const;
  fire_and_forget IsCursorCaptureEnabled(bool value);
  bool CaptureClientArea() const;
  fire_and_forget CaptureClientArea(bool value);
  hstring ExecutablePathPattern() const;
  fire_and_forget ExecutablePathPattern(hstring);
  hstring WindowClass() const;
  fire_and_forget WindowClass(hstring);

 private:
  std::shared_ptr<VDXROVR::WindowCaptureTab> GetTab() const;
};

struct TabUIDataTemplateSelector
  : TabUIDataTemplateSelectorT<TabUIDataTemplateSelector> {
  TabUIDataTemplateSelector() = default;

  winrt::Microsoft::UI::Xaml::DataTemplate Generic();
  void Generic(winrt::Microsoft::UI::Xaml::DataTemplate const& value);
  winrt::Microsoft::UI::Xaml::DataTemplate Browser();
  void Browser(winrt::Microsoft::UI::Xaml::DataTemplate const& value);
  winrt::Microsoft::UI::Xaml::DataTemplate DCSRadioLog();
  void DCSRadioLog(winrt::Microsoft::UI::Xaml::DataTemplate const& value);
  winrt::Microsoft::UI::Xaml::DataTemplate WindowCapture();
  void WindowCapture(winrt::Microsoft::UI::Xaml::DataTemplate const& value);

  DataTemplate SelectTemplateCore(const IInspectable&);
  DataTemplate SelectTemplateCore(const IInspectable&, const DependencyObject&);

 private:
  DataTemplate mGeneric;
  DataTemplate mBrowser;
  DataTemplate mDCSRadioLog;
  DataTemplate mWindowCapture;
};

}// namespace winrt::VDXROVRApp::implementation
namespace winrt::VDXROVRApp::factory_implementation {
struct TabsSettingsPage
  : TabsSettingsPageT<TabsSettingsPage, implementation::TabsSettingsPage> {};

struct TabUIData : TabUIDataT<TabUIData, implementation::TabUIData> {};
struct BrowserTabUIData
  : BrowserTabUIDataT<BrowserTabUIData, implementation::BrowserTabUIData> {};
struct DCSRadioLogTabUIData : DCSRadioLogTabUIDataT<
                                DCSRadioLogTabUIData,
                                implementation::DCSRadioLogTabUIData> {};
struct WindowCaptureTabUIData : WindowCaptureTabUIDataT<
                                  WindowCaptureTabUIData,
                                  implementation::WindowCaptureTabUIData> {};

struct TabUIDataTemplateSelector
  : TabUIDataTemplateSelectorT<
      TabUIDataTemplateSelector,
      implementation::TabUIDataTemplateSelector> {};

}// namespace winrt::VDXROVRApp::factory_implementation
