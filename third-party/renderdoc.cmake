
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/renderdoc")

add_library(renderdoc INTERFACE)
target_include_directories(renderdoc INTERFACE "${SOURCE_DIR}/include")
add_library(ThirdParty::RenderDoc ALIAS renderdoc)

include(vdxrovr_add_license_file)
vdxrovr_add_license_file(
  "${SOURCE_DIR}/LICENSE.md"
  "LICENSE-ThirdParty-RenderDoc.txt"
)
