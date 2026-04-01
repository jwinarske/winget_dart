## 0.1.0

- Initial release.
- Complete C++/WinRT bridge (`winget_nc.dll`) for Windows x64 and ARM64.
- Flat C ABI with 17 exported functions covering lifecycle, search, install,
  upgrade, uninstall, simulate, catalogs, updates, and cancellation.
- Dart public API: `WgClient`, `WgTransaction<T>`, typed models and exceptions.
- `WingetBridge` abstraction for testability with `FakeWingetBridge`.
- Dart Build Hook compiles the native DLL via CMake at `dart pub get` time.
- FFI bindings with `@Native` annotations resolved via `CodeAsset`.
- Exponential backoff retry in `WgClient.connect()` for fresh-login race.
- Sequential install guard preventing concurrent mutating operations.
- JSON message protocol with discriminator-based decoding.
- Google Test suite for C++ codec and bridge functions.
- Dart unit tests with `FakeWingetBridge` (no DLL or WinGet required).
- Integration test suite for live Windows runners.
- GitHub Actions CI: C++ tests, Dart unit tests, integration, coverage.
