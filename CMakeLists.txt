cmake_minimum_required(VERSION 3.0)
project(i3-windows
  VERSION 0.1.0
)
set(CMAKE_CXX_STANDARD 17)

add_subdirectory(third-party/i3ipc++)
include_directories(
  ${I3IPCpp_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/include
)
link_directories(${I3IPCpp_LIBRARY_DIRS})

option(BUILD_TESTS "Build the tests" ON)
if(BUILD_TESTS)
    enable_testing()
    include(GoogleTest)
    add_subdirectory(tests)
endif()

set(SRCS
  src/WindowManager.cc
)

add_library(i3w
  ${SRCS}
)

target_link_libraries(i3w
  ${I3IPCpp_LIBRARIES}
)

add_executable(i3-windows
  src/main.cc
)

target_link_libraries(i3-windows
  i3w
)