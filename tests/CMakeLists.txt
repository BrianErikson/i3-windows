
# ref https://cliutils.gitlab.io/modern-cmake/chapters/testing/googletest.html
add_subdirectory("${PROJECT_SOURCE_DIR}/third-party/googletest" "third-party/googletest")
mark_as_advanced(
  BUILD_GMOCK BUILD_GTEST BUILD_SHARED_LIBS
  gmock_build_tests gtest_build_samples gtest_build_tests
  gtest_disable_pthreads gtest_force_shared_crt gtest_hide_internal_symbols
)
set_target_properties(gtest PROPERTIES FOLDER third-party)
set_target_properties(gtest_main PROPERTIES FOLDER third-party)
set_target_properties(gmock PROPERTIES FOLDER third-party)
set_target_properties(gmock_main PROPERTIES FOLDER third-party)

add_executable(unittests
  test.cc
)

target_link_libraries(unittests
  gtest
  gmock
  gtest_main
  i3w
)

gtest_discover_tests(unittests
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

set_target_properties(unittests PROPERTIES FOLDER tests)
