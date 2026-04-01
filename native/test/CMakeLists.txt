# Google Test suite for winget_nc bridge — no Dart runtime required.

add_executable(bridge_test
  bridge_test.cpp
)

target_include_directories(bridge_test PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/third_party/dart
  ${CMAKE_SOURCE_DIR}/third_party/winget_interop/include
)

target_link_libraries(bridge_test PRIVATE
  GTest::gtest_main
  winget_nc
)

include(GoogleTest)
gtest_discover_tests(bridge_test)
