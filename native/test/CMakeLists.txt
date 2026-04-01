# Google Test suite for winget_nc bridge — no Dart runtime required.
#
# These tests link against the winget_nc shared library and override
# PostToDart with a mock to capture posted messages. Tests that require
# live WinRT/COM are guarded by #ifdef WINGET_LIVE_TESTS.
#
# Build: cmake -B build native/ -DBUILD_TESTING=ON -G "Visual Studio 17 2022" -A x64
# Run:   ctest --test-dir build/test -C Release --output-on-failure

add_executable(bridge_test
  bridge_test.cpp
)

target_include_directories(bridge_test PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/third_party
  ${CMAKE_SOURCE_DIR}/third_party/winget_interop/include
)

target_link_libraries(bridge_test PRIVATE
  GTest::gtest_main
  winget_nc
)

# Enable live WinRT tests when building on Windows with COM available.
# Consumers can set -DWINGET_LIVE_TESTS=ON to enable the full suite.
if(WINGET_LIVE_TESTS)
  target_compile_definitions(bridge_test PRIVATE WINGET_LIVE_TESTS)
endif()

include(GoogleTest)
gtest_discover_tests(bridge_test)
