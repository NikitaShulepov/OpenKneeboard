include(GNUInstallDirs)

function(add_helper_executable TARGET)
  set(options 32BIT_ONLY)
  set(valueArgs)
  set(multiValueArgs)
  cmake_parse_arguments(HELPER_ARG "${options}" "${valueArgs}" "${multiValueArgs}" ${ARGN})

  vdxrovr_add_executable(${TARGET} EXCLUDE_FROM_ALL RUNTIME_DEPENDENCY ${ARGN})
  if (HELPER_ARG_32BIT_ONLY AND NOT BUILD_IS_32BIT)
    return()
  endif ()
endfunction()

function(add_utility_executable TARGET)
  vdxrovr_add_executable(
    ${TARGET}
    RUNTIME_EXE
    OUTPUT_SUBDIR "utilities"
    ${ARGN}
  )
endfunction()

include(remote-control.cmake)

add_utility_executable(
  VDXROVR-Plugin-Tab-Action
  WIN32
  plugin-tab-action.cpp
)
target_link_libraries(
  VDXROVR-Plugin-Tab-Action
  PRIVATE
  VDXROVR-dprint
  VDXROVR-APIEvent
  VDXROVR-tracing
)

add_utility_executable(
  VDXROVR-RemoteControl-SET_TAB
  WIN32
  set-tab-remote.cpp
  remote-traceprovider.cpp
)
target_link_libraries(
  VDXROVR-RemoteControl-SET_TAB
  PRIVATE
  VDXROVR-dprint
  VDXROVR-APIEvent
  VDXROVR-tracing
)

add_utility_executable(
  VDXROVR-RemoteControl-SET_PROFILE
  WIN32
  set-profile-remote.cpp
  remote-traceprovider.cpp
)
target_link_libraries(
  VDXROVR-RemoteControl-SET_PROFILE
  PRIVATE
  VDXROVR-APIEvent
  VDXROVR-dprint
  VDXROVR-tracing
)

add_utility_executable(
  VDXROVR-Viewer
  WIN32
  viewer.cpp
  viewer-settings.cpp
  viewer-d3d11.cpp
  viewer-d3d12.cpp
  viewer-vulkan.cpp
  dds.hpp
  icon.rc
)
target_link_libraries(
  VDXROVR-Viewer
  PRIVATE
  VDXROVR-fatal
  VDXROVR-config
  VDXROVR-chrono
  VDXROVR-D2DErrorRenderer
  VDXROVR-D3D11
  VDXROVR-D3D12
  VDXROVR-Vulkan
  VDXROVR-DXResources
  VDXROVR-Filesystem
  VDXROVR-APIEvent
  VDXROVR-GetSystemColor
  VDXROVR-RenderDoc
  VDXROVR-SHM
  VDXROVR-SHM-Client-D3D11
  VDXROVR-SHM-Client-D3D12
  VDXROVR-SHM-Client-Vulkan
  VDXROVR-Viewer-Shaders
  VDXROVR-scope_exit
  System::D2d1
  System::D3d11
  System::Dcomp
  System::Dxgi
  ThirdParty::DirectXTK
)

add_helper_executable(
  VDXROVR-OpenXR-Helper
  WIN32
  openxr-helper.cpp
  icon.rc
)
target_link_libraries(
  VDXROVR-OpenXR-Helper
  PRIVATE
  VDXROVR-RuntimeFiles
  VDXROVR-dprint
)

add_helper_executable(
  VDXROVR-WindowCaptureHook-Helper
  WIN32
  32BIT_ONLY
  WindowCaptureHook-helper.cpp
)
target_link_libraries(
  VDXROVR-WindowCaptureHook-Helper
  PRIVATE
  VDXROVR-WindowCaptureControl
  VDXROVR-consolelib
  VDXROVR-dprint
  VDXROVR-shims
  System::Shell32
)

add_executable(capi-test capi-test.cpp)
target_include_directories(
  capi-test
  PRIVATE
  "$<TARGET_PROPERTY:VDXROVR-c-api,INTERFACE_INCLUDE_DIRECTORIES>")
set_property(
  TARGET capi-test
  PROPERTY CXX_STANDARD 17)

vdxrovr_add_executable(symbolize-traces symbolize-traces.cpp)
target_link_libraries(
  symbolize-traces
  PRIVATE
  VDXROVR-shims
  ThirdParty::DIA
  ThirdParty::WIL
)
set_target_properties(
  symbolize-traces
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${BUILD_OUT_PDBDIR}"
)
add_custom_command(
  TARGET symbolize-traces
  POST_BUILD
  COMMAND
  "${CMAKE_COMMAND}" -E copy_if_different
  "${DIASDK_DLL}"
  "$<TARGET_FILE_DIR:symbolize-traces>/"
)

add_executable(
  task-test
  task-test.cpp
)
target_link_libraries(
  task-test
  PRIVATE
  ThirdParty::WIL
  VDXROVR-task
)