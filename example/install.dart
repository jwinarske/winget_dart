import 'dart:io';
import 'package:winget_dart/winget_dart.dart';
import 'package:winget_dart/src/bridge/native_winget_bridge.dart';

Future<void> main(List<String> args) async {
  final packageId = args.isNotEmpty ? args[0] : 'Kitware.CMake';
  final client = await WgClient.connect(NativeWingetBridge());

  // Simulate first
  print('Resolving dependencies for $packageId...');
  final plan = await client.simulateInstall(packageId);
  if (plan.isEmpty) {
    print('$packageId is already installed.');
    await client.close();
    return;
  }
  print('  Installing: ${plan.installing.map((p) => p.id).join(', ')}');
  print('  Upgrading:  ${plan.upgrading.map((p) => p.id).join(', ')}');

  // Install
  print('Installing $packageId...');
  final tx = client.installPackage(packageId);
  await for (final p in tx.progress) {
    stdout.write('\r  ${p.percent}% ${p.label}    ');
  }
  await tx.result;
  print('\nDone.');

  await client.close();
}
