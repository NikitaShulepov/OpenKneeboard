vdxrovr_add_library(detours-ext STATIC detours-ext.cpp)
target_link_libraries(detours-ext PUBLIC ThirdParty::detours VDXROVR-dprint)

configure_file(
  VDXROVR-OpenXR.in.def
  VDXROVR-OpenXR${BUILD_BITNESS}.def
  @ONLY
  NEWLINE_STYLE UNIX
)
vdxrovr_add_library(
  VDXROVR-OpenXR
  MODULE
  DUALARCH
  VDXROVR-OpenXR${BUILD_BITNESS}.def
  OpenXRKneeboard.cpp
  OpenXRGestureTracker.cpp
  OpenXRMirror.cpp
  OpenXRD3D11Kneeboard.cpp
  OpenXRD3D12Kneeboard.cpp
  OpenXRVulkanKneeboard.cpp
  OpenXRNext.cpp
)
target_link_libraries(
  VDXROVR-OpenXR
  PRIVATE
  ThirdParty::OpenXR
  ThirdParty::DirectXTK
  ThirdParty::DirectXTK12
  ThirdParty::VulkanHeaders
  VDXROVR-D3D11
  VDXROVR-D3D12
  VDXROVR-Vulkan
  VDXROVR-Elevation
  VDXROVR-RayIntersectsRect
  VDXROVR-SHM
  VDXROVR-SHM-Client-D3D11
  VDXROVR-SHM-Client-D3D12
  VDXROVR-SHM-Client-Vulkan
  VDXROVR-SpriteAtlas
  VDXROVR-StateMachine
  VDXROVR-APIEvent
  VDXROVR-UserAction
  VDXROVR-VRKneeboard
  VDXROVR-config
  VDXROVR-dprint
  VDXROVR-scope_exit
  VDXROVR-shims
  VDXROVR-version
)

configure_file(
  VDXROVR-OpenXR.in.json
  VDXROVR-OpenXR.intermediate.json
  NEWLINE_STYLE UNIX
)

# Keep the un-suffixed name to keep compatibility with previous installations
set(64BIT_OPENXR_JSON "${BUILD_OUT_BINDIR}/VDXROVR-OpenXR.json")
set(32BIT_OPENXR_JSON "${BUILD_OUT_BINDIR}/VDXROVR-OpenXR32.json")
if (BUILD_IS_32BIT)
  set(OPENXR_JSON_OUT "${32BIT_OPENXR_JSON}")
else ()
  set(OPENXR_JSON_OUT "${64BIT_OPENXR_JSON}")
endif ()
file(
  GENERATE
  OUTPUT "${OPENXR_JSON_OUT}"
  INPUT "${CMAKE_CURRENT_BINARY_DIR}/VDXROVR-OpenXR.intermediate.json"
  NEWLINE_STYLE UNIX
)

vdxrovr_add_library(VDXROVR-WindowCaptureHook MODULE DUALARCH WindowCaptureHook.cpp)
target_link_libraries(
  VDXROVR-WindowCaptureHook
  PRIVATE
  VDXROVR-config
  VDXROVR-tracing
  detours-ext
)
set_target_properties(
  VDXROVR-WindowCaptureHook
  PROPERTIES

  # Needed to avoid C function name mangling on 32-bit builds
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)
