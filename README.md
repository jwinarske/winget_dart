# winget_dart

Typed Dart API for the [Windows Package Manager](https://github.com/microsoft/winget-cli)
via the `Microsoft.Management.Deployment` COM/WinRT interface.

Supports search, install, upgrade, uninstall, and simulate operations with
async progress streams. Targets Windows x64 and ARM64.

## Platform support

| Platform          | Search | Install / Upgrade / Uninstall | Simulate |
|-------------------|--------|-------------------------------|----------|
| Windows 10 x64    | Yes    | Yes                           | Yes      |
| Windows 11 x64    | Yes    | Yes                           | Yes      |
| Windows 11 ARM64  | Yes    | Yes                           | Yes      |
| macOS / Linux     | --     | --                            | --       |

## Prerequisites

WinGet ships in-box as part of App Installer on:

- Windows 10 1809+ (x64)
- Windows 11 (x64 and ARM64)

On older machines or Windows Server, install manually from
[WinGet releases](https://github.com/microsoft/winget-cli/releases).

### Build requirements

The native bridge DLL (`winget_nc.dll`) is compiled automatically by
the Dart Build Hook during `dart pub get` / `dart run` / `dart test`.
The following must be installed on the build machine:

- **CMake** >= 3.21
- **Visual Studio 2022 Build Tools** with:
  - MSVC v143 C++ x64/x86 build tools
  - MSVC v143 C++ ARM64 build tools (for ARM64 target)
  - Windows SDK 10.0.19041 or later
- **vcpkg** with `cppwinrt` (optional; the Windows SDK built-in headers are used as fallback)

## Quick start

```dart
import 'package:winget_dart/winget_dart.dart';
import 'package:winget_dart/src/bridge/native_winget_bridge.dart';

Future<void> main() async {
  final client = await WgClient.connect(NativeWingetBridge());

  // Search for packages
  final tx = client.searchName('cmake');
  await for (final pkg in tx.packages) {
    print('${pkg.id} ${pkg.version}');
  }

  // Simulate then install
  final plan = await client.simulateInstall('Kitware.CMake');
  print('Will install: ${plan.installing.map((p) => p.id).join(', ')}');

  final install = client.installPackage('Kitware.CMake');
  await for (final p in install.progress) {
    print('${p.percent}% ${p.label}');
  }
  await install.result;

  await client.close();
}
```

## API overview

### WgClient

The primary entry point. Create via `WgClient.connect(bridge)` where `bridge`
is a `WingetBridge` implementation:

- `NativeWingetBridge()` for production (requires the native DLL)
- `FakeWingetBridge()` for unit testing (pure Dart, no DLL needed)

| Method              | Returns                        | Description                        |
|---------------------|--------------------------------|------------------------------------|
| `searchName(query)` | `WgTransaction<List<WgPackage>>`| Stream packages matching query     |
| `findById(id)`      | `Future<WgPackage?>`           | Exact ID lookup                    |
| `listCatalogs()`    | `Future<List<WgCatalog>>`      | All configured catalogs            |
| `listInstalled()`   | `WgTransaction<List<WgPackage>>`| All installed packages             |
| `simulateInstall()` | `Future<WgInstallPlan>`        | Dry-run dependency resolution      |
| `installPackage()`  | `WgTransaction<void>`          | Install with progress events       |
| `upgradePackage()`  | `WgTransaction<void>`          | Upgrade with progress events       |
| `uninstallPackage()`| `WgTransaction<void>`          | Uninstall with progress events     |
| `getUpdates()`      | `WgTransaction<List<WgPackage>>`| Packages with available upgrades   |
| `cancel()`          | `void`                         | Cancel any in-flight operation     |

### WgTransaction\<T\>

Wraps an async operation with three access points:

- `packages` -- stream of `WgPackage` results (search/list operations)
- `progress` -- stream of `WgProgress` events (install/upgrade/uninstall)
- `result` -- future that completes when the operation finishes

### Simulate-first pattern

Always call `simulateInstall` before `installPackage` to show the user what
will change. The simulate and install are separate WinGet transactions --
the actual install re-resolves dependencies independently.

```dart
final plan = await client.simulateInstall('Kitware.CMake');
if (!plan.isEmpty) {
  print('Installing: ${plan.installing.map((p) => p.id).join(', ')}');
  await client.installPackage('Kitware.CMake').result;
}
```

## Testing

Unit tests use `FakeWingetBridge` and run without the native DLL or WinGet:

```bash
dart test --exclude-tags=integration
```

Integration tests require a live Windows environment with WinGet:

```bash
dart test --tags=integration
```

## Sharp edges

- **Elevation detection**: The bridge auto-detects process elevation via
  `GetTokenInformation(TokenElevation)` and selects the correct COM factory
  CLSID. Using the wrong factory causes a hard crash (not an exception).

- **Parallel install prohibition**: WinGet's COM API does not support
  concurrent install operations. The bridge enforces a sequential guard;
  attempting a second install/upgrade/uninstall returns an error immediately.

- **Fresh-login race**: On freshly provisioned machines, `WgClient.connect()`
  supports retry with exponential backoff via `maxRetries` and `retryDelay`
  parameters.

- **COM apartment threading**: All WinRT calls run on a dedicated MTA thread.
  `Dart_PostCObject_DL` is safe to call from any thread.

- **ARM64**: Requires a native ARM64 DLL -- x64 emulation does not apply to
  COM server activation. Windows 11 build 22000+ required on ARM64.

## Architecture

```
Dart (WgClient)
  |  dart:ffi  (@Native -- resolved via Build Hook CodeAsset)
  v
winget_bridge.h  (flat C ABI -- Windows x64 and ARM64)
  |
  +-- WgManager         (COM MTA apartment thread, factory selection)
  |
  +-- WgTransaction     (per-operation context, cancellation)
        |
        |  C++/WinRT coroutines   IAsyncOperationWithProgress<>
        v
  Microsoft.Management.Deployment  COM server
  (part of App Installer / WinGet)
        |
        v
  WinGet catalogs  (winget community repo, msstore, ...)
```

## Building the native library manually

```bash
# x64
cmake -B build native/ -G "Visual Studio 17 2022" -A x64 -DTARGET_ARCH=x64
cmake --build build --config Release

# ARM64 (cross-compile on x64 host)
cmake -B build-arm64 native/ -G "Visual Studio 17 2022" -A ARM64 -DTARGET_ARCH=arm64
cmake --build build-arm64 --config Release
```

The Dart Build Hook (`hook/build.dart`) runs these commands automatically.

## License

Apache-2.0
