vdxrovr_add_library(VDXROVR-Lib-Headers INTERFACE)
target_include_directories(VDXROVR-Lib-Headers INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/include/VDXROVR/config.in.hpp"
  "${CMAKE_CURRENT_BINARY_DIR}/include/VDXROVR/config.hpp"
  @ONLY
)
file(
  GENERATE
  OUTPUT
  "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/include/VDXROVR/detail/config.hpp"
  CONTENT "\
namespace VDXROVR::detail::Config {
  constexpr auto BuildType = \"$<CONFIG>\";
}"
)
vdxrovr_add_library(VDXROVR-config INTERFACE)
target_include_directories(
  VDXROVR-config
  INTERFACE
  "${CMAKE_CURRENT_BINARY_DIR}/include"
  "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/include"
)
target_link_libraries(VDXROVR-config INTERFACE VDXROVR-Lib-Headers)

include(Geometry2D.cmake)

vdxrovr_add_library(VDXROVR-ThreadGuard STATIC ThreadGuard.cpp)
target_link_libraries(VDXROVR-ThreadGuard PUBLIC VDXROVR-Lib-Headers)
target_link_libraries(VDXROVR-ThreadGuard PRIVATE VDXROVR-dprint)

vdxrovr_add_library(
  VDXROVR-inplace_vector
  INTERFACE
  HEADERS
  include/VDXROVR/inplace_vector.hpp
  INCLUDE_DIRECTORIES
  include
)
target_link_libraries(VDXROVR-inplace_vector INTERFACE VDXROVR-fatal)

vdxrovr_add_library(
  VDXROVR-SpriteAtlas
  STATIC
  SpriteAtlas.cpp
  HEADERS
  include/VDXROVR/SpriteAtlas.hpp
  INCLUDE_DIRECTORIES
  include
)
target_link_libraries(
  VDXROVR-SpriteAtlas
  PUBLIC
  VDXROVR-config
  VDXROVR-inplace_vector
  ThirdParty::RectangleBinPack
)


vdxrovr_add_library(
  VDXROVR-fatal
  STATIC
  fatal.cpp
  HEADERS
  include/VDXROVR/fatal.hpp
  INCLUDE_DIRECTORIES
  include
)
target_link_libraries(
  VDXROVR-fatal
  PUBLIC
  VDXROVR-config
  PRIVATE
  VDXROVR-Elevation
  VDXROVR-dprint
  VDXROVR-shims
  VDXROVR-Filesystem
  VDXROVR-version
  ThirdParty::CppWinRT
  ThirdParty::WIL
  ThirdParty::detours
  ThirdParty::felly
)

vdxrovr_add_library(VDXROVR-GetMainHWND STATIC GetMainHWND.cpp)
target_link_libraries(
  VDXROVR-GetMainHWND
  PUBLIC
  VDXROVR-Lib-Headers
  VDXROVR-win32
)
target_link_libraries(
  VDXROVR-GetMainHWND
  PRIVATE
  VDXROVR-config
  VDXROVR-shims
)

vdxrovr_add_library(VDXROVR-GetSystemColor STATIC GetSystemColor.cpp)
target_link_libraries(
  VDXROVR-GetSystemColor
  PUBLIC
  VDXROVR-Lib-Headers)

vdxrovr_add_library(VDXROVR-handles INTERFACE)
target_link_libraries(
  VDXROVR-handles
  INTERFACE
  VDXROVR-Lib-Headers
)

vdxrovr_add_library(VDXROVR-Elevation STATIC Elevation.cpp)
target_link_libraries(
  VDXROVR-Elevation
  PUBLIC
  VDXROVR-Lib-Headers
)
target_link_libraries(
  VDXROVR-Elevation
  PRIVATE
  VDXROVR-config
  VDXROVR-dprint
  VDXROVR-scope_exit
  ThirdParty::CppWinRT
  System::Shell32
)

vdxrovr_add_library(VDXROVR-UTF8 STATIC utf8.cpp)
target_link_libraries(
  VDXROVR-UTF8
  PUBLIC
  ThirdParty::JSON
  VDXROVR-Lib-Headers
  PRIVATE
  VDXROVR-fatal
)

vdxrovr_add_library(VDXROVR-DebugTimer STATIC DebugTimer.cpp)
target_link_libraries(
  VDXROVR-DebugTimer
  PRIVATE
  VDXROVR-dprint
)
target_link_libraries(
  VDXROVR-DebugTimer
  PUBLIC
  VDXROVR-Lib-Headers
)

vdxrovr_add_library(VDXROVR-tracing STATIC tracing.cpp)
target_link_libraries(
  VDXROVR-tracing
  PRIVATE
  VDXROVR-dprint
  ThirdParty::CppWinRT
)
target_link_libraries(
  VDXROVR-tracing
  PUBLIC
  VDXROVR-Lib-Headers
)

vdxrovr_add_library(
  VDXROVR-D3D11
  STATIC
  D3D11.cpp
  D3D11/SpriteBatch.cpp
)
target_link_libraries(
  VDXROVR-D3D11
  PUBLIC
  System::D3d11
  VDXROVR-shims
  ThirdParty::DirectXTK
  ThirdParty::CppWinRT
  VDXROVR-Lib-Headers
  VDXROVR-Sprite-DXBC
  PRIVATE
  VDXROVR-dprint
  System::Dxguid
)

vdxrovr_add_library(
  VDXROVR-D3D12
  STATIC
  D3D12/SpriteBatch.cpp
)
target_link_libraries(
  VDXROVR-D3D12
  PUBLIC
  System::D3d12
  VDXROVR-shims
  ThirdParty::DirectXTK12
  ThirdParty::CppWinRT
  VDXROVR-Lib-Headers
  VDXROVR-SpriteBatch-DXIL
  PRIVATE
  VDXROVR-dprint
  VDXROVR-RenderDoc
  System::Dxguid
)

vdxrovr_add_library(
  VDXROVR-Vulkan
  STATIC
  Vulkan.cpp
  Vulkan/SpriteBatch.cpp)
target_link_libraries(
  VDXROVR-Vulkan
  PUBLIC
  VDXROVR-dprint
  ThirdParty::VulkanHeaders
  VDXROVR-Lib-Headers
  VDXROVR-SpriteBatch-SPIRV
)

vdxrovr_add_library(VDXROVR-DXResources STATIC DXResources.cpp)
target_link_libraries(
  VDXROVR-DXResources
  PUBLIC
  VDXROVR-Lib-Headers
  VDXROVR-D3D11
  PRIVATE
  VDXROVR-dprint
  System::Gdi32
)

set(RUNTIME_FILES_CPP "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/RuntimeFiles.cpp")
file(GENERATE OUTPUT "${RUNTIME_FILES_CPP}" INPUT RuntimeFiles.cpp.in)
vdxrovr_add_library(
  VDXROVR-RuntimeFiles
  STATIC
  "${RUNTIME_FILES_CPP}"
  RuntimeFilesGetInstallationDirectory.cpp
)
target_link_libraries(
  VDXROVR-RuntimeFiles
  PUBLIC
  VDXROVR-Lib-Headers
  PRIVATE
  VDXROVR-dprint
  VDXROVR-Filesystem
)
if (ENABLE_APP)
  set(RUNTIME_FILES_APP_CPP "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/RuntimeFiles-app.cpp")
  file(GENERATE OUTPUT "${RUNTIME_FILES_APP_CPP}" INPUT RuntimeFiles-app.cpp.in)
  target_sources(VDXROVR-RuntimeFiles PRIVATE "${RUNTIME_FILES_APP_CPP}")
endif ()

vdxrovr_add_library(
  VDXROVR-SHM
  STATIC
  SHM.cpp
  SHM/ActiveConsumers.cpp
)
target_link_libraries(
  VDXROVR-SHM
  PUBLIC
  VDXROVR-dprint
  VDXROVR-Lib-Headers
  VDXROVR-StateMachine
  PRIVATE
  VDXROVR-Elevation
  VDXROVR-config
  VDXROVR-fatal
  VDXROVR-shims
  VDXROVR-version
  VDXROVR-win32
)

vdxrovr_add_library(
  VDXROVR-SHM-Client-D3D11
  STATIC
  SHM/D3D11.cpp
  D3D11/Renderer.cpp
)
target_link_libraries(
  VDXROVR-SHM-Client-D3D11
  PUBLIC
  VDXROVR-shims
  VDXROVR-SHM
  System::D3d11
  ThirdParty::CppWinRT
  ThirdParty::DirectXTK
  PRIVATE
  VDXROVR-dprint
)

vdxrovr_add_library(
  VDXROVR-SHM-Client-D3D12
  STATIC
  SHM/D3D12.cpp
  D3D12/Renderer.cpp
)
target_link_libraries(
  VDXROVR-SHM-Client-D3D12
  PUBLIC
  VDXROVR-shims
  VDXROVR-SHM
  VDXROVR-D3D12
  System::D3d12
  ThirdParty::CppWinRT
  ThirdParty::DirectXTK12
  PRIVATE
  VDXROVR-dprint
)

vdxrovr_add_library(
  VDXROVR-SHM-Client-Vulkan
  STATIC
  SHM/Vulkan.cpp
)
target_link_libraries(
  VDXROVR-SHM-Client-Vulkan
  PUBLIC
  VDXROVR-SHM
  VDXROVR-Vulkan
  VDXROVR-shims
  ThirdParty::CppWinRT
  ThirdParty::VulkanHeaders
)

vdxrovr_add_library(
  VDXROVR-chrono
  STATIC
  chrono.cpp
  HEADERS
  include/VDXROVR/chrono.hpp
  INCLUDE_DIRECTORIES
  include
)

vdxrovr_add_library(
  VDXROVR-dprint
  STATIC
  dprint.cpp
  HEADERS
  include/VDXROVR/dprint.hpp
  INCLUDE_DIRECTORIES
  include
)

target_link_libraries(
  VDXROVR-dprint
  PUBLIC
  VDXROVR-config
  VDXROVR-fatal
  VDXROVR-scope_exit
  ThirdParty::bindline
)
target_link_libraries(VDXROVR-dprint
  PRIVATE
  VDXROVR-scope_exit
)

vdxrovr_add_library(VDXROVR-consolelib STATIC ConsoleLoopCondition.cpp)
target_link_libraries(
  VDXROVR-consolelib
  PUBLIC
  VDXROVR-Lib-Headers
  PRIVATE
  VDXROVR-fatal
  VDXROVR-win32
)

vdxrovr_add_library(VDXROVR-APIEvent STATIC APIEvent.cpp)
target_link_libraries(VDXROVR-APIEvent
  PUBLIC
  VDXROVR-Lib-Headers
  VDXROVR-UTF8
  VDXROVR-json
  PRIVATE
  VDXROVR-config
  VDXROVR-dprint
)

vdxrovr_add_library(VDXROVR-D2DErrorRenderer STATIC D2DErrorRenderer.cpp)
target_link_libraries(
  VDXROVR-D2DErrorRenderer
  PUBLIC
  VDXROVR-DXResources
  VDXROVR-UTF8
  VDXROVR-config
  System::D2d1
  System::Dwrite
  VDXROVR-Lib-Headers)

vdxrovr_add_library(VDXROVR-RayIntersectsRect STATIC RayIntersectsRect.cpp)
target_link_libraries(
  VDXROVR-RayIntersectsRect
  PUBLIC
  VDXROVR-Lib-Headers
  ThirdParty::DirectXTK)
target_link_libraries(
  VDXROVR-RayIntersectsRect
  PRIVATE
  VDXROVR-dprint)

vdxrovr_add_library(VDXROVR-StateMachine INTERFACE)
target_link_libraries(
  VDXROVR-StateMachine
  INTERFACE
  VDXROVR-Lib-Headers
  VDXROVR-dprint
  ThirdParty::magic_enum
)

vdxrovr_add_library(VDXROVR-scope_exit INTERFACE)
target_link_libraries(
  VDXROVR-scope_exit
  INTERFACE
  VDXROVR-Lib-Headers
  ThirdParty::felly
)

vdxrovr_add_library(VDXROVR-VRKneeboard STATIC VRKneeboard.cpp)
target_link_libraries(
  VDXROVR-VRKneeboard
  PUBLIC
  VDXROVR-Lib-Headers
  ThirdParty::DirectXTK
  VDXROVR-RayIntersectsRect
  VDXROVR-SHM
)

set(VERSION_CPP_FILE "${CMAKE_CURRENT_BINARY_DIR}/include/VDXROVR/version.hpp")
add_custom_target(
  VDXROVR-version-sources
  COMMAND
  ${CMAKE_COMMAND}
  -DVERSION_MAJOR=${CMAKE_PROJECT_VERSION_MAJOR}
  -DVERSION_MINOR=${CMAKE_PROJECT_VERSION_MINOR}
  -DVERSION_PATCH=${CMAKE_PROJECT_VERSION_PATCH}
  -DVERSION_BUILD=${CMAKE_PROJECT_VERSION_TWEAK}
  -DRELEASE_NAME=${RELEASE_NAME}
  -DOPENXR_API_LAYER_IMPLEMENTATION_VERSION=${OPENXR_API_LAYER_IMPLEMENTATION_VERSION}
  "-DINPUT_CPP_FILE=${CMAKE_CURRENT_SOURCE_DIR}/include/VDXROVR/version.in.hpp"
  "-DOUTPUT_CPP_FILE=${VERSION_CPP_FILE}"
  "-P${CMAKE_CURRENT_SOURCE_DIR}/../version.cmake"
  DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/../version.cmake"
  "version.hpp.in"
  BYPRODUCTS
  "${VERSION_CPP_FILE}"
  WORKING_DIRECTORY
  "${CMAKE_SOURCE_DIR}"
)
vdxrovr_add_library(VDXROVR-version INTERFACE)
target_include_directories(
  VDXROVR-version
  INTERFACE
  "${CMAKE_CURRENT_BINARY_DIR}/include"
)
add_dependencies(VDXROVR-version VDXROVR-version-sources)
vdxrovr_add_library(
  VDXROVR-semver
  STATIC
  semver.cpp
  HEADERS
  include/VDXROVR/semver.hpp
  INCLUDE_DIRECTORIES
  include
)
target_link_libraries(
  VDXROVR-semver
  PRIVATE
  VDXROVR-dprint
  ThirdParty::SemVer
)

vdxrovr_add_library(VDXROVR-shims INTERFACE)
target_include_directories(
  VDXROVR-shims
  INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
  VDXROVR-shims
  INTERFACE
  VDXROVR-dprint
  VDXROVR-fatal
)

vdxrovr_add_library(VDXROVR-json STATIC json.cpp)
target_link_libraries(
  VDXROVR-json
  PUBLIC
  VDXROVR-dprint
  ThirdParty::JSON
  VDXROVR-Lib-Headers
)

vdxrovr_add_library(VDXROVR-Filesystem STATIC Filesystem.cpp)
target_link_libraries(
  VDXROVR-Filesystem
  PUBLIC
  VDXROVR-shims
  VDXROVR-config
  VDXROVR-Lib-Headers
  PRIVATE
  VDXROVR-StateMachine
  VDXROVR-UTF8
  VDXROVR-dprint
  VDXROVR-fatal
  System::Shlwapi
  ThirdParty::CppWinRT
  ThirdParty::WIL
)

vdxrovr_add_library(VDXROVR-WindowCaptureControl STATIC WindowCaptureControl.cpp)
target_link_libraries(
  VDXROVR-WindowCaptureControl
  PRIVATE
  VDXROVR-RuntimeFiles
  VDXROVR-dprint
  VDXROVR-shims
)
target_link_libraries(
  VDXROVR-WindowCaptureControl
  PUBLIC
  VDXROVR-handles
  VDXROVR-Lib-Headers
)

vdxrovr_add_library(
  VDXROVR-RenderDoc
  STATIC
  RenderDoc.cpp
)
target_link_libraries(
  VDXROVR-RenderDoc
  PUBLIC
  VDXROVR-Lib-Headers
  PRIVATE
  VDXROVR-fatal
  ThirdParty::RenderDoc
)
vdxrovr_add_library(
  VDXROVR-EnumerateProcesses
  STATIC
  EnumerateProcesses.cpp
  HEADERS
  include/VDXROVR/EnumerateProcesses.hpp
  INCLUDE_DIRECTORIES
  include
)
target_link_libraries(
  VDXROVR-EnumerateProcesses
  PRIVATE
  VDXROVR-dprint
  VDXROVR-tracing
)

vdxrovr_add_library(
  VDXROVR-win32
  INTERFACE
  HEADERS
  include/VDXROVR/Win32.hpp
  INCLUDE_DIRECTORIES
  include
)
target_link_libraries(
  VDXROVR-win32
  INTERFACE
  VDXROVR-fatal
  VDXROVR-shims
  ThirdParty::bindline
  ThirdParty::CppWinRT
)

add_library(VDXROVR-task INTERFACE)
target_link_libraries(
  VDXROVR-task
  INTERFACE
  VDXROVR-StateMachine
  VDXROVR-dprint
  VDXROVR-fatal
  VDXROVR-shims
  ThirdParty::CppWinRT
  ThirdParty::magic_enum
)

vdxrovr_add_library(
  VDXROVR-UserAction
  INTERFACE
  HEADERS include/VDXROVR/UserAction.hpp
  INCLUDE_DIRECTORIES include
)