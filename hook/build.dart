// hook/build.dart
// SPDX-License-Identifier: Apache-2.0
//
// Compiles winget_nc.dll via CMake and registers it as a CodeAsset.
// Supports Windows x64 (native or cross) and Windows ARM64 (native or cross).
// Runs at build time on the consumer's machine.
//
// If the native build fails (missing dependencies, no MSVC, etc.), the hook
// returns without adding a CodeAsset. The @Native FFI functions will throw
// at call time if the DLL is not available, but unit tests using
// FakeWingetBridge are unaffected.

import 'dart:io';
import 'package:hooks/hooks.dart';
import 'package:code_assets/code_assets.dart';

void main(List<String> args) async {
  await build(args, (input, output) async {
    if (!input.config.buildCodeAssets) return;

    final os = input.config.code.targetOS;
    if (os != OS.windows) {
      // winget_dart only supports Windows. Skip silently on other platforms.
      return;
    }

    final arch = input.config.code.targetArchitecture;

    final (cmakeArch, targetArch) = switch (arch) {
      Architecture.x64 => ('x64', 'x64'),
      Architecture.arm64 => ('ARM64', 'arm64'),
      _ => throw UnsupportedError(
          'winget_dart does not support architecture: $arch'),
    };

    final nativeDir = input.packageRoot.resolve('native/');

    // Use a short temp path for the cmake build directory. The default
    // output directory under .dart_tool/hooks_runner/... exceeds MAX_PATH
    // on Windows when combined with cmake's internal paths.
    final outputHash = input.outputDirectory.pathSegments
        .join()
        .hashCode
        .toUnsigned(32)
        .toRadixString(16);
    final buildDir = Directory('${Platform.environment['TEMP'] ?? r'C:\Temp'}'
        r'\wg_nc_'
        '$outputHash'
        '_$targetArch');
    await buildDir.create(recursive: true);

    // Locate the WinGet COM interop DLL from the NuGet package.
    // Search _nuget/ relative to package root, then fall back to
    // the global NuGet cache.
    String? interopDll;
    final archDir = targetArch == 'arm64' ? 'win-arm64' : 'win-x64';
    final nugetDir = Directory.fromUri(input.packageRoot.resolve('_nuget/'));
    if (nugetDir.existsSync()) {
      for (final entry in nugetDir.listSync()) {
        if (entry is Directory &&
            entry.path.contains('Microsoft.WindowsPackageManager.ComInterop')) {
          final dll = File(
              '${entry.path}/bin/$archDir/native/static/Microsoft.Management.Deployment.dll');
          if (dll.existsSync()) {
            interopDll = dll.path;
            break;
          }
        }
      }
    }
    // Fall back to user-level NuGet cache.
    if (interopDll == null) {
      final home = Platform.environment['USERPROFILE'] ?? '';
      final cacheBase = Directory(
          '$home/.nuget/packages/microsoft.windowspackagemanager.cominterop');
      if (cacheBase.existsSync()) {
        final versions = cacheBase.listSync()
          ..sort((a, b) => b.path.compareTo(a.path));
        for (final v in versions) {
          final dll = File(
              '${v.path}/bin/$archDir/native/static/Microsoft.Management.Deployment.dll');
          if (dll.existsSync()) {
            interopDll = dll.path;
            break;
          }
        }
      }
    }

    final cmakeInteropArg = interopDll != null
        ? ['-DWINGET_INTEROP_DLL=${interopDll.replaceAll('\\', '/')}']
        : <String>[];

    // Configure CMake with Visual Studio 2022 generator.
    final configure = await Process.run(
        'cmake',
        [
          nativeDir.toFilePath(),
          '-G',
          'Visual Studio 17 2022',
          '-A',
          cmakeArch,
          '-DCMAKE_BUILD_TYPE=Release',
          '-DTARGET_ARCH=$targetArch',
          '-DBUILD_TESTING=OFF',
          '-DCMAKE_INSTALL_PREFIX=${buildDir.path}/install',
          ...cmakeInteropArg,
        ],
        workingDirectory: buildDir.path);
    if (configure.exitCode != 0) {
      stderr.writeln(
          'winget_dart: cmake configure failed (native bridge unavailable):\n'
          '${configure.stderr}');
      return;
    }

    // Build.
    final buildResult = await Process.run(
        'cmake',
        [
          '--build',
          '.',
          '--config',
          'Release',
          '--parallel',
        ],
        workingDirectory: buildDir.path);
    if (buildResult.exitCode != 0) {
      stderr.writeln(
          'winget_dart: cmake build failed (native bridge unavailable):\n'
          '${buildResult.stderr}');
      return;
    }

    // Install.
    final installResult = await Process.run(
        'cmake',
        [
          '--install',
          '.',
          '--config',
          'Release',
        ],
        workingDirectory: buildDir.path);
    if (installResult.exitCode != 0) {
      stderr.writeln(
          'winget_dart: cmake install failed (native bridge unavailable):\n'
          '${installResult.stderr}');
      return;
    }

    final dll = Uri.file('${buildDir.path}/install/bin/winget_nc.dll');

    output.assets.code.add(CodeAsset(
      package: 'winget_dart',
      name: 'src/winget_nc.dart',
      file: dll,
      linkMode: DynamicLoadingBundled(),
    ));

    // Also register the WinGet COM interop DLL as a bundled asset.
    // winget_nc.dll loads it at runtime from its own directory.
    final interopInstalled = File(
        '${buildDir.path}/install/bin/Microsoft.Management.Deployment.dll');
    if (interopInstalled.existsSync()) {
      output.assets.code.add(CodeAsset(
        package: 'winget_dart',
        name: 'src/winget_interop.dart',
        file: interopInstalled.uri,
        linkMode: DynamicLoadingBundled(),
      ));
    }
  });
}
