cmake_minimum_required(VERSION 3.21)
project(winget_nc C CXX)

# winget_dart bridge DLL — Windows x64 and ARM64.
# Requires MSVC (C++/WinRT requires MSVC ABI).
# Architecture is selected by the caller (build hook) via -A x64 or -A ARM64
# when using the Visual Studio generator.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Detect target architecture (set by the build hook via -DTARGET_ARCH=x64|arm64).
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

# Dart SDK headers for Dart_PostCObject_DL.
# Headers are at third_party/dart/dart_api_dl.h and included as "dart/dart_api_dl.h",
# so the include path must be the parent directory.
set(DART_SDK_DIR "${CMAKE_SOURCE_DIR}/third_party")

add_library(winget_nc SHARED
  src/winget_bridge.cpp
  src/winget_manager.cpp
  src/winget_transaction.cpp
  src/message_codec.cpp
  "${DART_SDK_DIR}/dart/dart_api_dl.c"
)

target_include_directories(winget_nc PRIVATE
  src/
  "${WINGET_INTEROP_DIR}/include"
  "${DART_SDK_DIR}"
)

# C++/WinRT: use the Windows SDK's built-in C++/WinRT projection headers.
# Windows SDK 10.0.17763+ includes cppwinrt.exe and base projection headers.
# If cppwinrt is available via vcpkg, prefer it; otherwise fall back to the SDK.
find_package(cppwinrt CONFIG QUIET)
if(cppwinrt_FOUND)
  message(STATUS "winget_nc: using vcpkg cppwinrt")
  target_link_libraries(winget_nc PRIVATE Microsoft::CppWinRT)
else()
  message(STATUS "winget_nc: using Windows SDK C++/WinRT headers")
  # The Windows SDK ships C++/WinRT headers under the 'cppwinrt' include subdir.
  # MSVC automatically includes the Windows SDK paths, so no extra include needed.
endif()

target_link_libraries(winget_nc PRIVATE
  windowsapp    # WinRT runtime import library
  ole32         # CoCreateInstance
  oleaut32
  runtimeobject # RoActivateInstance, RoGetActivationFactory
  wintrust      # WinVerifyTrust (Authenticode signature verification)
)

# WinGet COM activation uses RoActivateInstance at runtime — no import lib
# required. The COM interop DLL (Microsoft.Management.Deployment.dll) is
# loaded by the WinRT activation factory, not linked at build time.
# If a WindowsPackageManager.lib is vendored, link it; otherwise skip.
if(EXISTS "${WINGET_INTEROP_DIR}/lib/${TARGET_ARCH}/WindowsPackageManager.lib")
  target_link_libraries(winget_nc PRIVATE
    "${WINGET_INTEROP_DIR}/lib/${TARGET_ARCH}/WindowsPackageManager.lib"
  )
else()
  message(STATUS "winget_nc: No import lib for ${TARGET_ARCH} — using WinRT activation only")
endif()

# Export only the flat C ABI symbols
set_target_properties(winget_nc PROPERTIES
  WINDOWS_EXPORT_ALL_SYMBOLS OFF
  CXX_VISIBILITY_PRESET hidden
)

target_compile_definitions(winget_nc PRIVATE
  WINGET_NC_EXPORTS
  NOMINMAX
  WIN32_LEAN_AND_MEAN
)

# C++/WinRT coroutines on MSVC
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
  RUNTIME DESTINATION bin
)

# Install the WinGet COM interop DLL alongside winget_nc.dll.
# The build hook sets WINGET_INTEROP_DLL to the path of
# Microsoft.Management.Deployment.dll from the NuGet package.
if(DEFINED WINGET_INTEROP_DLL AND EXISTS "${WINGET_INTEROP_DLL}")
  install(FILES "${WINGET_INTEROP_DLL}" DESTINATION bin)
  message(STATUS "winget_nc: will install interop DLL from ${WINGET_INTEROP_DLL}")
endif()
