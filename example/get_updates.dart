import 'package:winget_dart/winget_dart.dart';
import 'package:winget_dart/src/bridge/native_winget_bridge.dart';

Future<void> main() async {
  final client = await WgClient.connect(NativeWingetBridge());

  print('Checking for updates...');
  final tx = client.getUpdates();
  int count = 0;
  await for (final pkg in tx.packages) {
    print('  ${pkg.id.padRight(40)} '
        '${pkg.version} -> ${pkg.availableVersion}');
    count++;
  }
  if (count == 0) print('All packages are up to date.');

  await client.close();
}
