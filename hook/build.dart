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
      Architecture.x64   => ('x64',   'x64'),
      Architecture.arm64 => ('ARM64', 'arm64'),
      _ => throw UnsupportedError(
              'winget_dart does not support architecture: $arch'),
    };

    final nativeDir = input.packageRoot.resolve('native/');
    final buildDir = Directory.fromUri(
        input.outputDirectory.resolve('winget_nc_build_$targetArch/'));
    await buildDir.create(recursive: true);

    // Configure CMake with Visual Studio 2022 generator.
    final configure = await Process.run('cmake', [
      nativeDir.toFilePath(),
      '-G', 'Visual Studio 17 2022',
      '-A', cmakeArch,
      '-DCMAKE_BUILD_TYPE=Release',
      '-DTARGET_ARCH=$targetArch',
      '-DBUILD_TESTING=OFF',
      '-DCMAKE_INSTALL_PREFIX=${buildDir.path}/install',
    ], workingDirectory: buildDir.path);
    if (configure.exitCode != 0) {
      stderr.writeln(
          'winget_dart: cmake configure failed (native bridge unavailable):\n'
          '${configure.stderr}');
      return;
    }

    // Build.
    final buildResult = await Process.run('cmake', [
      '--build', '.',
      '--config', 'Release',
      '--parallel',
    ], workingDirectory: buildDir.path);
    if (buildResult.exitCode != 0) {
      stderr.writeln(
          'winget_dart: cmake build failed (native bridge unavailable):\n'
          '${buildResult.stderr}');
      return;
    }

    // Install.
    final installResult = await Process.run('cmake', [
      '--install', '.',
      '--config', 'Release',
    ], workingDirectory: buildDir.path);
    if (installResult.exitCode != 0) {
      stderr.writeln(
          'winget_dart: cmake install failed (native bridge unavailable):\n'
          '${installResult.stderr}');
      return;
    }

    final dll = Uri.file(
        '${buildDir.path}/install/bin/winget_nc.dll');

    output.assets.code.add(CodeAsset(
      package: 'winget_dart',
      name: 'src/winget_nc.dart',
      file: dll,
      linkMode: DynamicLoadingBundled(),
    ));
  });
}
