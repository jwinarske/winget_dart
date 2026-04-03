// Demonstrates search, install simulation, and catalog listing
// using the winget_dart package.

import 'package:winget_dart/winget_dart.dart';
import 'package:winget_dart/src/bridge/native_winget_bridge.dart';

Future<void> main() async {
  // Connect to WinGet via the native bridge.
  final client = await WgClient.connect(NativeWingetBridge());

  // List available catalogs.
  final catalogs = await client.listCatalogs();
  for (final c in catalogs) {
    print('Catalog: ${c.name} (${c.id})');
  }

  // Search for packages by name.
  final tx = client.searchName('cmake');
  await for (final pkg in tx.packages) {
    print('${pkg.id} ${pkg.version}');
  }

  // Simulate an install to preview what would happen.
  final plan = await client.simulateInstall('Kitware.CMake');
  if (!plan.isEmpty) {
    print('Would install: ${plan.installing.map((p) => p.id).join(', ')}');
  }

  await client.close();
}
