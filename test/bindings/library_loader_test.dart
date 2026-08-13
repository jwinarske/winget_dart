import 'dart:io';

import 'package:path/path.dart' as p;
import 'package:test/test.dart';
import 'package:winget_dart/src/bindings/library_loader.dart';

void main() {
  late Directory tmpDir;

  setUp(() {
    tmpDir = Directory.systemTemp.createTempSync('wg_loader_test_');
  });

  tearDown(() {
    tmpDir.deleteSync(recursive: true);
  });

  /// Create a fake DLL file at [path] and return the file.
  File placeDll(String path) {
    final f = File(path);
    f.parent.createSync(recursive: true);
    f.writeAsBytesSync([0x4D, 0x5A]); // MZ header stub
    return f;
  }

  group('resolveWingetNcPath', () {
    test('returns overridePath when set', () {
      final dll = placeDll(p.join(tmpDir.path, 'override', 'winget_nc.dll'));
      final result = resolveWingetNcPath(overridePath: dll.path);
      expect(result, dll.path);
    });

    test('ignores empty overridePath', () {
      final result = resolveWingetNcPath(
        overridePath: '',
        environment: const {},
      );
      // Falls through — no other candidates, so null.
      expect(result, isNull);
    });

    test('returns WINGET_NC_LIB env var when set', () {
      final dll = placeDll(p.join(tmpDir.path, 'env', 'winget_nc.dll'));
      final result = resolveWingetNcPath(
        environment: {'WINGET_NC_LIB': dll.path},
      );
      expect(result, dll.path);
    });

    test('ignores empty WINGET_NC_LIB', () {
      final result = resolveWingetNcPath(environment: {'WINGET_NC_LIB': ''});
      expect(result, isNull);
    });

    test('finds DLL in .dart_tool/lib/', () {
      final dartToolLib = p.join(tmpDir.path, '.dart_tool', 'lib');
      placeDll(p.join(dartToolLib, 'winget_nc.dll'));
      final packageConfig = p.join(
        tmpDir.path,
        '.dart_tool',
        'package_config.json',
      );
      File(packageConfig)
        ..parent.createSync(recursive: true)
        ..writeAsStringSync('{}');

      final result = resolveWingetNcPath(
        packageConfig: packageConfig,
        environment: const {},
      );
      expect(result, endsWith('winget_nc.dll'));
      expect(result, contains('.dart_tool'));
    });

    test('finds DLL in temp build-hook output', () {
      final buildDir = Directory(p.join(tmpDir.path, 'wg_nc_abcd1234_x64'));
      placeDll(p.join(buildDir.path, 'install', 'bin', 'winget_nc.dll'));

      final result = resolveWingetNcPath(
        environment: {'TEMP': tmpDir.path},
        hostArch: 'x64',
      );
      expect(result, endsWith('winget_nc.dll'));
      expect(result, contains('wg_nc_'));
    });

    test('picks most recent temp build when multiple exist', () {
      // Create two build dirs with different timestamps.
      final old = placeDll(
        p.join(
          tmpDir.path,
          'wg_nc_old11111_arm64',
          'install',
          'bin',
          'winget_nc.dll',
        ),
      );
      // Ensure different mtime.
      sleep(const Duration(milliseconds: 50));
      final newer = placeDll(
        p.join(
          tmpDir.path,
          'wg_nc_new22222_arm64',
          'install',
          'bin',
          'winget_nc.dll',
        ),
      );

      final result = resolveWingetNcPath(
        environment: {'TEMP': tmpDir.path},
        hostArch: 'arm64',
      );
      expect(result, newer.path);
      expect(result, isNot(old.path));
    });

    test('skips temp dirs that do not match hostArch', () {
      placeDll(
        p.join(
          tmpDir.path,
          'wg_nc_abc_arm64',
          'install',
          'bin',
          'winget_nc.dll',
        ),
      );

      final result = resolveWingetNcPath(
        environment: {'TEMP': tmpDir.path},
        hostArch: 'x64', // looking for x64, but only arm64 exists
      );
      expect(result, isNull);
    });

    test('finds DLL next to executable', () {
      final exeDir = p.join(tmpDir.path, 'bin');
      placeDll(p.join(exeDir, 'winget_nc.dll'));

      final result = resolveWingetNcPath(
        environment: const {},
        resolvedExecutable: p.join(exeDir, 'myapp.exe'),
      );
      expect(result, endsWith('winget_nc.dll'));
      expect(result, contains('bin'));
    });

    test('finds DLL in lib/ next to executable', () {
      final exeDir = p.join(tmpDir.path, 'bin');
      placeDll(p.join(exeDir, 'lib', 'winget_nc.dll'));

      final result = resolveWingetNcPath(
        environment: const {},
        resolvedExecutable: p.join(exeDir, 'myapp.exe'),
      );
      expect(result, endsWith('winget_nc.dll'));
      expect(result, contains('lib'));
    });

    test('returns null when no candidate is found', () {
      final result = resolveWingetNcPath(
        environment: {'TEMP': tmpDir.path},
        hostArch: 'x64',
      );
      expect(result, isNull);
    });

    test('override takes precedence over env var', () {
      final overrideDll = placeDll(
        p.join(tmpDir.path, 'override', 'winget_nc.dll'),
      );
      final envDll = placeDll(p.join(tmpDir.path, 'env', 'winget_nc.dll'));

      final result = resolveWingetNcPath(
        overridePath: overrideDll.path,
        environment: {'WINGET_NC_LIB': envDll.path},
      );
      expect(result, overrideDll.path);
    });

    test('env var takes precedence over .dart_tool', () {
      final envDll = placeDll(p.join(tmpDir.path, 'env', 'winget_nc.dll'));
      final dartToolLib = p.join(tmpDir.path, '.dart_tool', 'lib');
      placeDll(p.join(dartToolLib, 'winget_nc.dll'));
      final packageConfig = p.join(
        tmpDir.path,
        '.dart_tool',
        'package_config.json',
      );
      File(packageConfig).writeAsStringSync('{}');

      final result = resolveWingetNcPath(
        environment: {'WINGET_NC_LIB': envDll.path},
        packageConfig: packageConfig,
      );
      expect(result, envDll.path);
    });
  });

  group('findDartToolLib', () {
    test('finds via packageConfig path', () {
      final dartToolLib = p.join(tmpDir.path, '.dart_tool', 'lib');
      Directory(dartToolLib).createSync(recursive: true);
      final packageConfig = p.join(
        tmpDir.path,
        '.dart_tool',
        'package_config.json',
      );
      File(packageConfig).writeAsStringSync('{}');

      final result = findDartToolLib(packageConfig: packageConfig);
      expect(result, dartToolLib);
    });

    test('finds via file: URI packageConfig', () {
      final dartToolLib = p.join(tmpDir.path, '.dart_tool', 'lib');
      Directory(dartToolLib).createSync(recursive: true);
      final packageConfig = p.join(
        tmpDir.path,
        '.dart_tool',
        'package_config.json',
      );
      File(packageConfig).writeAsStringSync('{}');

      final result = findDartToolLib(
        packageConfig: Uri.file(packageConfig).toString(),
      );
      expect(result, dartToolLib);
    });

    test('walks up from currentDir to find .dart_tool/lib', () {
      final dartToolLib = p.join(tmpDir.path, '.dart_tool', 'lib');
      Directory(dartToolLib).createSync(recursive: true);
      final nested = p.join(tmpDir.path, 'a', 'b', 'c');
      Directory(nested).createSync(recursive: true);

      final result = findDartToolLib(currentDir: nested);
      expect(result, dartToolLib);
    });

    test('returns null when nothing found', () {
      final result = findDartToolLib(
        packageConfig: null,
        currentDir: tmpDir.path,
      );
      expect(result, isNull);
    });

    test('returns null for empty packageConfig', () {
      final result = findDartToolLib(packageConfig: '');
      expect(result, isNull);
    });
  });
}
