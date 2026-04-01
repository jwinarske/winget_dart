## 0.1.0

Initial release.

### Features

- Complete C++/WinRT bridge (`winget_nc.dll`) for Windows x64 and ARM64
- Flat C ABI with 17 exported functions: lifecycle, search, install,
  upgrade, uninstall, simulate, catalogs, updates, and cancellation
- Dart public API: `WgClient`, `WgTransaction<T>`, typed models and exceptions
- `WingetBridge` abstraction for testability with `FakeWingetBridge`
- Dart Build Hook compiles the native DLL via CMake at build time
- FFI bindings with `@Native` annotations resolved via `CodeAsset`
- Exponential backoff retry in `WgClient.connect()` for fresh-login race
- Sequential install guard preventing concurrent mutating operations
- JSON message protocol with discriminator-based decoding
- COM factory selection via elevation detection (standard vs elevated CLSID)
- C++/WinRT coroutines with `IAsyncOperationWithProgress` progress streaming

### Testing

- Dart unit tests with `FakeWingetBridge` (no DLL or WinGet required)
- Integration test suite for live Windows runners
- Google Test suite for C++ codec and bridge functions
- GitHub Actions CI: C++ builds, Dart unit tests, integration, coverage

### Known limitations

- Version pinning for install/upgrade is not yet implemented (always installs latest)
- Simulate returns the target package only (WinGet COM API lacks a public
  dependency resolution endpoint)
- Uninstall requires a WinGet COM API version that includes `UninstallPackageAsync`
