import 'package:winget_dart/winget_dart.dart';
import 'package:winget_dart/src/bridge/native_winget_bridge.dart';

Future<void> main(List<String> args) async {
  final packageId = args.isNotEmpty ? args[0] : 'Kitware.CMake';
  final client = await WgClient.connect(NativeWingetBridge());

  print('Simulating install of $packageId...');
  final plan = await client.simulateInstall(packageId);

  if (plan.isEmpty) {
    print('Nothing to do — $packageId is already satisfied.');
  } else {
    if (plan.installing.isNotEmpty) {
      print('Would install:');
      for (final p in plan.installing) {
        print('  ${p.id} ${p.version}');
      }
    }
    if (plan.upgrading.isNotEmpty) {
      print('Would upgrade:');
      for (final p in plan.upgrading) {
        print('  ${p.id} ${p.version}');
      }
    }
    if (plan.removing.isNotEmpty) {
      print('Would remove:');
      for (final p in plan.removing) {
        print('  ${p.id} ${p.version}');
      }
    }
  }

  await client.close();
}
