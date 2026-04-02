// Live integration test — requires real Windows + WinGet.
// Run with: dart test --tags=integration
// Skipped in CI unless RUN_INTEGRATION_TESTS=1.
@Tags(['integration'])
library;

import 'package:test/test.dart';
import 'package:winget_dart/winget_dart.dart';
import 'package:winget_dart/src/bridge/native_winget_bridge.dart';

void main() {
  test('connect succeeds', () async {
    final client = await WgClient.connect(NativeWingetBridge());
    addTearDown(client.close);
    expect(client, isNotNull);
  });

  test('list catalogs returns at least one catalog', () async {
    final client = await WgClient.connect(NativeWingetBridge());
    addTearDown(client.close);
    final catalogs = await client.listCatalogs();
    expect(catalogs, isNotEmpty);
  });

  test('search cmake finds Kitware.CMake', () async {
    final client = await WgClient.connect(NativeWingetBridge());
    addTearDown(client.close);
    final tx = client.searchName('cmake');
    final packages = await tx.result;
    expect(packages.map((p) => p.id), contains('Kitware.CMake'));
  });

  test('list installed returns at least one package', () async {
    final client = await WgClient.connect(NativeWingetBridge());
    addTearDown(client.close);
    final packages = await client.listInstalled().result;
    expect(packages, isNotEmpty);
  });

  test('simulate install resolves plan', () async {
    final client = await WgClient.connect(NativeWingetBridge());
    addTearDown(client.close);
    final plan = await client.simulateInstall('Kitware.CMake');
    // Plan may be empty if already installed; just verify it doesn't throw.
    expect(plan, isNotNull);
  });
}
