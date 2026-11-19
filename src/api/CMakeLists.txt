# Output filenames use underscores rather than hyphens for
# increased compatibility; in particular, LUA requires the DLL
# name matches a C function name.

vdxrovr_add_library(
  VDXROVR-lua-api
  MODULE
  DUALARCH
  OUTPUT_NAME "VDXROVR_LuaAPI"
  VDXROVR_LuaAPI.cpp)
target_link_libraries(
  VDXROVR-lua-api
  PUBLIC
  ThirdParty::Lua::DelayLoad
  PRIVATE
  VDXROVR-dprint
  VDXROVR-tracing
  VDXROVR-APIEvent
)
# Install to a separate directory as for lua, we generally
# want to modify `cpath`; let's not pollute cpath with
# all the other dlls that VDXROVR needs on
set_target_properties(
  VDXROVR-lua-api
  PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${BUILD_OUT_ROOT}/lib/lua"
)

vdxrovr_add_library(
  VDXROVR-c-api
  MODULE
  DUALARCH
  OUTPUT_NAME "VDXROVR_CAPI"
  VDXROVR_CAPI.cpp)
target_link_libraries(
  VDXROVR-c-api
  PRIVATE
  VDXROVR-dprint
  VDXROVR-tracing
  VDXROVR-APIEvent)
target_include_directories(
  VDXROVR-c-api
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR})
file(
  GENERATE
  OUTPUT
  "${BUILD_OUT_ROOT}/include/VDXROVR_CAPI.h"
  INPUT
  "VDXROVR_CAPI.h"
)
