set(GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/gen/$<CONFIG>/include")
set(GENERATED_HEADER_DIR "${GENERATED_INCLUDE_DIR}/VDXROVR/Shaders/gen")
make_directory("${GENERATED_HEADER_DIR}")


macro(add_shader_with_ms_compiler COMPILER TARGET ENTRYPOINT SOURCE)
  set(SHADER_OUTPUT "${GENERATED_HEADER_DIR}/${TARGET}.hpp")
  add_custom_command(
    OUTPUT "${SHADER_OUTPUT}"
    DEPENDS "${SOURCE}"
    COMMAND
    "${COMPILER}"
    "$<$<CONFIG:Debug>:-O0>"
    "$<$<CONFIG:Debug>:-Zi>"
    -Fh "${SHADER_OUTPUT}"
    -E ${ENTRYPOINT}
    "${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE}"
    ${ARGN}
  )
  add_library("${TARGET}" INTERFACE "${SHADER_OUTPUT}")
  target_include_directories("${TARGET}" INTERFACE "${GENERATED_INCLUDE_DIR}")
endmacro()

macro(add_dxc_shader TARGET ENTRYPOINT SOURCE)
  add_shader_with_ms_compiler(ThirdParty::dxc "${TARGET}" "${ENTRYPOINT}" "${SOURCE}" ${ARGN})
endmacro()

macro(add_fxc_shader TARGET ENTRYPOINT SOURCE)
  add_shader_with_ms_compiler(System::fxc "${TARGET}" "${ENTRYPOINT}" "${SOURCE}" ${ARGN})
endmacro()

add_dxc_shader(
  VDXROVR-SpriteBatch-SPIRV-PS
  SpritePixelShader
  SpriteBatch.hlsl
  -spirv -T ps_6_0
  -D VK=1
)

add_dxc_shader(
  VDXROVR-SpriteBatch-SPIRV-VS
  SpriteVertexShader
  SpriteBatch.hlsl
  -spirv -T vs_6_0
  -D VK=1
)

add_library(VDXROVR-SpriteBatch-SPIRV INTERFACE)
target_link_libraries(
  VDXROVR-SpriteBatch-SPIRV
  INTERFACE
  VDXROVR-SpriteBatch-SPIRV-PS
  VDXROVR-SpriteBatch-SPIRV-VS
)
target_include_directories(
  VDXROVR-SpriteBatch-SPIRV
  INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

add_dxc_shader(
  VDXROVR-SpriteBatch-DXIL-PS
  SpritePixelShader
  SpriteBatch.hlsl
  -T ps_5_1
)

add_dxc_shader(
  VDXROVR-SpriteBatch-DXIL-VS
  SpriteVertexShader
  SpriteBatch.hlsl
  -T vs_5_1
)

add_library(VDXROVR-SpriteBatch-DXIL INTERFACE)
target_link_libraries(
  VDXROVR-SpriteBatch-DXIL
  INTERFACE
  VDXROVR-SpriteBatch-DXIL-PS
  VDXROVR-SpriteBatch-DXIL-VS
)
target_include_directories(
  VDXROVR-SpriteBatch-DXIL
  INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

add_fxc_shader(
  VDXROVR-Sprite-DXBC-PS
  SpritePixelShader
  Sprite.hlsl
  -T ps_5_0
)
add_fxc_shader(
  VDXROVR-Sprite-DXBC-VS
  SpriteVertexShader
  Sprite.hlsl
  -T vs_5_0
)
add_library(VDXROVR-Sprite-DXBC INTERFACE)
target_link_libraries(
  VDXROVR-Sprite-DXBC
  INTERFACE
  VDXROVR-Sprite-DXBC-PS
  VDXROVR-Sprite-DXBC-VS
)
target_include_directories(
  VDXROVR-Sprite-DXBC
  INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# Shader Model 5.0 targets D3D11.0 and above
add_fxc_shader(
  VDXROVR-Viewer-DXBC-VS
  ViewerVertexShader
  Viewer.hlsl
  -T vs_5_0
)
add_fxc_shader(
  VDXROVR-Viewer-DXBC-PS
  ViewerPixelShader
  Viewer.hlsl
  -T ps_5_0
)
add_library(VDXROVR-Viewer-Shaders INTERFACE)
target_link_libraries(
  VDXROVR-Viewer-Shaders
  INTERFACE
  VDXROVR-Viewer-DXBC-VS
  VDXROVR-Viewer-DXBC-PS
)
target_include_directories(
  VDXROVR-Viewer-Shaders
  INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)