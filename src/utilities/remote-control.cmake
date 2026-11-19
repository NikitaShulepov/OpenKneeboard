find_package(magic_args CONFIG REQUIRED)
include(MagicArgs)

add_utility_executable(
  VDXROVR-RemoteControl
  WIN32
  remote-control.cpp
  simple-remotes.cpp
  simple-remotes.hpp
  remote-traceprovider.cpp
)
target_link_libraries(
  VDXROVR-RemoteControl
  PRIVATE
  VDXROVR-APIEvent
  VDXROVR-UserAction
  VDXROVR-dprint
  VDXROVR-tracing
  magic_args::magic_args
)
magic_args_enumerate_subcommands(
  VDXROVR-RemoteControl
  HARDLINKS_DIR "$<TARGET_FILE_DIR:VDXROVR-RemoteControl>"
  TEXT_FILE "$<TARGET_FILE_DIR:VDXROVR-RemoteControl>/$<TARGET_FILE_BASE_NAME:VDXROVR-RemoteControl>-aliases.txt"
)
