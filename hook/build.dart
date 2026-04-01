// hook/build.dart
// SPDX-License-Identifier: Apache-2.0
//
// Compiles winget_nc.dll via CMake and registers it as a CodeAsset.
// Supports Windows x64 (native or cross) and Windows ARM64 (native or cross).
// Runs at `dart pub get` / build time on the consumer's machine.
//
// Prerequisites on the build machine:
//   - CMake >= 3.21
//   - Visual Studio 2022 Build Tools with:
//     - MSVC v143 x64/x86 build tools
//     - MSVC v143 ARM64 build tools (for ARM64 target)
//     - Windows SDK 10.0.19041+
//   - vcpkg with cppwinrt:x64-windows and/or cppwinrt:arm64-windows

import 'dart:io';
import 'package:hooks/hooks.dart';
import 'package:code_assets/code_assets.dart';

void main(List<String> args) async {
  await build(args, (input, output) async {
    final os = input.target.os;

    // winget_dart only supports Windows.
    if (os != OS.windows) {
      // On non-Windows platforms, add a data asset marker so consumers
      // can detect at runtime that the native bridge is unavailable.
      output.assets.data.add(DataAsset(
        package: 'winget_dart',
        name: 'platform_supported',
        file: Uri.file('assets/not_supported.txt'),
      ));
      return;
    }

    final arch = input.target.architecture;

    // Map Dart Architecture to CMake -A flag (VS generator) and to the
    // arch string used for the import lib subdirectory.
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

    // Use the Visual Studio 2022 generator with an explicit -A flag.
    // This is the most reliable way to select the MSVC toolchain for a
    // specific target architecture on both native and cross builds.
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
      throw Exception('cmake configure failed:\n${configure.stderr}');
    }

    // Build with --config Release (required for VS multi-config generators).
    final buildResult = await Process.run('cmake', [
      '--build', '.',
      '--config', 'Release',
      '--parallel',
    ], workingDirectory: buildDir.path);
    if (buildResult.exitCode != 0) {
      throw Exception('cmake build failed:\n${buildResult.stderr}');
    }

    // Install to a known location for the CodeAsset file URI.
    final installResult = await Process.run('cmake', [
      '--install', '.',
      '--config', 'Release',
    ], workingDirectory: buildDir.path);
    if (installResult.exitCode != 0) {
      throw Exception('cmake install failed:\n${installResult.stderr}');
    }

    final dll = Uri.file(
        '${buildDir.path}/install/bin/winget_nc.dll');

    output.assets.code.add(CodeAsset(
      package: 'winget_dart',
      name: 'src/winget_nc.dart',
      file: dll,
      linkMode: DynamicLoadingBundled(),
      os: OS.windows,
      architecture: arch,
    ));
  });
}
