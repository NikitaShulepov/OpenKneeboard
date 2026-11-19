vdxrovr_add_library(VDXROVR-Geometry2D INTERFACE include/VDXROVR/Geometry2D.hpp)
target_link_libraries(VDXROVR-Geometry2D INTERFACE ThirdParty::felly)
target_include_directories(VDXROVR-Geometry2D INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")