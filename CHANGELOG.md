## 0.4.0

- **Breaking:** migrate the native build hook from `hooks` 1.x to 2.x (and
  `code_assets` to 1.2.x). hooks 2.x requires **Dart >=3.10**, so the SDK lower
  bound rises from 3.6.0 — consumers on Dart 3.6–3.9 should stay on 0.3.x. The
  hook's behaviour is unchanged (it still builds `winget_nc.dll` via CMake and
  skips cleanly when its prerequisites are absent); the migration is a dependency
  bump only.
- Drop the `native_toolchain_c` dependency: it was declared but never imported
  (the hook drives CMake directly rather than `CBuilder`).

## 0.3.1

### Fixes

- Fix native DLL not found when run via `dart pub global activate` /
  `dart pub global run`. The `@Native` / `@DefaultAsset` code-assets
  approach only resolves when the hooks runner is active; globally
  activated packages skip it. Replaced with explicit
  `DynamicLibrary.open()` and a multi-strategy search path matching the
  pattern used by `packagekit_dart`.
- Add `resolveWingetNcPath()` with fallback chain: programmatic override →
  `WINGET_NC_LIB` env var → `.dart_tool/lib/` → `%TEMP%\wg_nc_*` build-hook
  output → next to executable → OS search path.
- Export `setWingetLibraryPath()` for consumers that vendor the DLL.

### Testing

- Add 18 unit tests for `library_loader.dart` covering all resolution
  strategies, precedence ordering, arch filtering, and edge cases.

## 0.3.0

### Improvements

- Add dartdoc comments to all public API elements (models, exceptions,
  `WingetBridge` interface, and `WgClient` methods)
- Add `example/example.dart` for pub.dev example tab
- Shorten package description to meet pub.dev 60–180 character guideline
- Bump minimum SDK constraint to `>=3.6.0` for build hook support

## 0.2.0

### Features

- Unpackaged COM activation for non-MSIX (desktop/console) apps
- Microsoft.Management.Deployment interop DLL bundling for x64 and ARM64
- Flutter example app (`example/winget_catalog`) with Fluent UI, Riverpod,
  and live WinGet integration (search, install, upgrade, uninstall, updates)
- README badges for pub.dev, CI status, Codecov, and license
- Screenshot of Flutter example in README

### CI & quality

- Codecov integration with `CODECOV_TOKEN`, `fail_ci_if_error`, and
  `codecov.yml` for server-side generated-file exclusion
- Coverage threshold enforcement at 60% in CI
- `--report-on=lib` and generated-file filtering in lcov pipeline
- `.pubignore` to exclude build artifacts and Flutter example from published package
- `.gitignore` at project root
- Exclude Flutter example from root `dart analyze` scope
- Unit test coverage improved from 89.6% to 97.2%: added tests for
  retry backoff, negative connect handle, listCatalogs/simulateInstall errors,
  cancel, streaming cancelled/error paths, model `toString()` methods,
  and `MessageDecoder.messageToString` unexpected type

### Breaking changes

- `FakeWingetBridge.stubIsAvailableFn` added; internal `_available` field
  changed from `bool` to `bool Function()` (no public API impact)

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
