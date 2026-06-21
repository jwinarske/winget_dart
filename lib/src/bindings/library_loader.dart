// library_loader.dart — DynamicLibrary.open() resolution for winget_nc.dll.
//
// The @Native / @DefaultAsset code-assets approach only works when the Dart VM
// has build-hook output available (dart run, dart test). It does NOT work for
// dart pub global activate / dart pub global run, because the hooks runner is
// not invoked for globally activated packages. This loader mirrors the pattern
// used by packagekit_dart: explicit DynamicLibrary.open() with a search path
// that covers all deployment layouts.

import 'dart:ffi';
import 'dart:io';

import 'package:path/path.dart' as p;

/// Optional explicit path to winget_nc.dll, set by [setWingetLibraryPath].
/// Takes precedence over every other resolution strategy. Must be set before
/// the first WgClient use (the library handle is loaded lazily on first access).
String? _overridePath;

/// Override the path used to load the native winget_nc library.
///
/// Useful for consumers that vendor a prebuilt DLL or need to point the loader
/// at a non-standard location. No effect once the library has already been
/// loaded.
void setWingetLibraryPath(String path) => _overridePath = path;

/// Lazily-loaded library handle. Once resolved, cached for the process lifetime.
DynamicLibrary? _cached;

/// Load the winget_nc native library, searching several well-known locations.
DynamicLibrary loadWingetNc() => _cached ??= _resolve();

DynamicLibrary _resolve() {
  // 0. Explicit programmatic override.
  if (_overridePath != null && _overridePath!.isNotEmpty) {
    return DynamicLibrary.open(_overridePath!);
  }

  // 1. Environment variable override.
  final envPath = Platform.environment['WINGET_NC_LIB'];
  if (envPath != null && envPath.isNotEmpty) {
    return DynamicLibrary.open(envPath);
  }

  // 2. Code-assets location: .dart_tool/lib/ in the consuming project.
  //    This is where `dart run` / `dart test` place the DLL via the build hook.
  final dartToolLib = _findDartToolLib();
  if (dartToolLib != null) {
    final dll = File(p.join(dartToolLib, 'winget_nc.dll'));
    if (dll.existsSync()) return DynamicLibrary.open(dll.path);
  }

  // 3. Build-hook temp output: %TEMP%\wg_nc_<hash>_<arch>\install\bin\.
  //    The build hook uses a short temp path to avoid MAX_PATH issues.
  //    Search for the most recent build.
  final tempDir = Platform.environment['TEMP'] ?? r'C:\Temp';
  final arch = _hostArch();
  final tempBuilds = <File>[];
  try {
    for (final entry in Directory(tempDir).listSync()) {
      if (entry is Directory &&
          entry.path.contains('wg_nc_') &&
          entry.path.endsWith('_$arch')) {
        final dll = File(p.join(entry.path, 'install', 'bin', 'winget_nc.dll'));
        if (dll.existsSync()) tempBuilds.add(dll);
      }
    }
  } on FileSystemException {
    // ignore — temp dir unreadable
  }
  if (tempBuilds.isNotEmpty) {
    // Pick the most recently modified build.
    tempBuilds
        .sort((a, b) => b.statSync().modified.compareTo(a.statSync().modified));
    return DynamicLibrary.open(tempBuilds.first.path);
  }

  // 4. Next to the running executable (AOT-compiled / bundled deployment).
  final exeDir = p.dirname(Platform.resolvedExecutable);
  for (final candidate in [
    p.join(exeDir, 'winget_nc.dll'),
    p.join(exeDir, 'lib', 'winget_nc.dll'),
  ]) {
    if (File(candidate).existsSync()) {
      return DynamicLibrary.open(candidate);
    }
  }

  // 5. Last resort: let the OS search PATH / system dirs.
  return DynamicLibrary.open('winget_nc.dll');
}

/// Best-effort discovery of .dart_tool/lib/ in the consuming project.
String? _findDartToolLib() {
  // The package config points into .dart_tool/.
  final pc = Platform.packageConfig;
  if (pc != null && pc.isNotEmpty) {
    final path = pc.startsWith('file:') ? Uri.parse(pc).toFilePath() : pc;
    // .../.dart_tool/package_config.json → .../.dart_tool
    final dir = p.dirname(path);
    if (p.basename(dir) == '.dart_tool') {
      final lib = p.join(dir, 'lib');
      if (Directory(lib).existsSync()) return lib;
    }
  }
  // Walk up from cwd.
  var dir = Directory.current;
  for (var i = 0; i < 8; i++) {
    final lib = p.join(dir.path, '.dart_tool', 'lib');
    if (Directory(lib).existsSync()) return lib;
    final parent = dir.parent;
    if (parent.path == dir.path) break;
    dir = parent;
  }
  return null;
}

String _hostArch() {
  final arch = Abi.current().toString();
  if (arch.contains('arm64') || arch.contains('ARM64')) return 'arm64';
  return 'x64';
}
