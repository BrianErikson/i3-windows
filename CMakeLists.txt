cmake_minimum_required(VERSION 3.0)
project(i3-windows
  VERSION 0.1.0
  DESCRIPTION "Save and restore i3 windows"
)
set(CMAKE_CXX_STANDARD 17)

add_subdirectory(third-party/i3ipc++)
include_directories(
  ${I3IPCpp_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/include
)
link_directories(${I3IPCpp_LIBRARY_DIRS})

find_package(X11 REQUIRED)
find_package(jsoncpp REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(XRANDR REQUIRED IMPORTED_TARGET xrandr)

option(BUILD_TESTS "Build the tests" ON)
if(BUILD_TESTS)
    enable_testing()
    include(GoogleTest)
    add_subdirectory(tests)
endif()

set(SRCS
  src/WindowManager.cc
  src/utils.cc
  src/i3WindowListener.cc
  src/I3Container.cc include/I3Container.h)

add_library(i3w
  ${SRCS}
)

target_link_libraries(i3w
  ${I3IPCpp_LIBRARIES}
  X11
  jsoncpp
  PkgConfig::XRANDR
)

target_include_directories(i3w
  PUBLIC ${CMAKE_BINARY_DIR}
)

add_executable(i3-windows
  src/main.cc
)

target_link_libraries(i3-windows
  i3w
)

configure_file(config.h.in config.h)
