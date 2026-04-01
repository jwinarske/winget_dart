cmake_minimum_required(VERSION 3.21)
project(winget_nc CXX)

# winget_dart bridge DLL — Windows x64 and ARM64.
# Requires MSVC or clang-cl (C++/WinRT requires MSVC ABI).
# Architecture is selected by the caller (build hook) via -A x64 or -A ARM64
# when using the Visual Studio generator, or via vcvarsall.bat when using Ninja.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Detect target architecture (set by the build hook via -DTARGET_ARCH=x64|arm64).
# Defaults to the host architecture when building natively.
if(NOT DEFINED TARGET_ARCH)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
    set(TARGET_ARCH "arm64")
  else()
    set(TARGET_ARCH "x64")
  endif()
endif()
message(STATUS "winget_nc: building for TARGET_ARCH=${TARGET_ARCH}")

# winget COM interop headers (vendored from NuGet)
set(WINGET_INTEROP_DIR "${CMAKE_SOURCE_DIR}/third_party/winget_interop")

# Dart SDK headers for Dart_PostCObject_DL
set(DART_SDK_DIR "${CMAKE_SOURCE_DIR}/third_party/dart")

add_library(winget_nc SHARED
  src/winget_bridge.cpp
  src/winget_manager.cpp
  src/winget_transaction.cpp
  src/message_codec.cpp
  "${DART_SDK_DIR}/dart_api_dl.c"   # Required for DL symbol resolution
)

target_include_directories(winget_nc PRIVATE
  src/
  "${WINGET_INTEROP_DIR}/include"
  "${DART_SDK_DIR}"
)

# C++/WinRT header-only projection (installed via vcpkg or NuGet)
find_package(cppwinrt CONFIG REQUIRED)
target_link_libraries(winget_nc PRIVATE
  Microsoft::CppWinRT
  windowsapp    # WinRT runtime
  ole32         # CoCreateInstance
  oleaut32
)

# Link the WinGet COM interop import lib — arch-specific subdirectory.
# The NuGet package ships lib/x64/WindowsPackageManager.lib and
# lib/arm64/WindowsPackageManager.lib.
target_link_libraries(winget_nc PRIVATE
  "${WINGET_INTEROP_DIR}/lib/${TARGET_ARCH}/WindowsPackageManager.lib"
)

# Export only the flat C ABI symbols
set_target_properties(winget_nc PROPERTIES
  WINDOWS_EXPORT_ALL_SYMBOLS OFF
  CXX_VISIBILITY_PRESET hidden
)

# Mark C ABI exports
target_compile_definitions(winget_nc PRIVATE
  WINGET_NC_EXPORTS
  NOMINMAX
  WIN32_LEAN_AND_MEAN
)

# winget_nc requires /await for C++/WinRT coroutines on MSVC
if(MSVC)
  target_compile_options(winget_nc PRIVATE /await:strict /W4 /EHsc)
endif()

# Google Test suite (no Dart runtime)
if(BUILD_TESTING)
  enable_testing()
  find_package(GTest CONFIG REQUIRED)
  add_subdirectory(test)
endif()

install(TARGETS winget_nc
  RUNTIME DESTINATION bin   # winget_nc.dll
)
