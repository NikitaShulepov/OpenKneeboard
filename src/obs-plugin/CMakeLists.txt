if (NOT BUILD_IS_64BIT)
  return()
endif ()

vdxrovr_add_library(
  VDXROVR-OBS-Mirror
  MODULE
  vdxr_mirror_obs.cpp
)

set_target_properties(
  VDXROVR-OBS-Mirror
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${BUILD_OUT_ROOT}/obs-plugin/bin/64bit"
  LIBRARY_OUTPUT_DIRECTORY "${BUILD_OUT_ROOT}/obs-plugin/bin/64bit"
)

target_link_libraries(
  VDXROVR-OBS-Mirror
  PRIVATE
  VDXROVR-Lib-Headers
  System::Mmdevapi
  System::Ole32
)

target_compile_options(
  VDXROVR-OBS-Mirror
  PRIVATE
  "/UNTDDI_VERSION"
  "/DNTDDI_VERSION=NTDDI_WIN10_FE"
)

add_custom_command(
  TARGET VDXROVR-OBS-Mirror
  POST_BUILD
  COMMAND
    "${CMAKE_COMMAND}" -E make_directory
    "${BUILD_OUT_ROOT}/obs-plugin"
  COMMAND
    "${CMAKE_COMMAND}" -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/README.txt"
    "${BUILD_OUT_ROOT}/obs-plugin/README.txt"
)
