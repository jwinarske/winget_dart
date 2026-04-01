import 'package:winget_dart/winget_dart.dart';
import 'package:winget_dart/src/bridge/native_winget_bridge.dart';

Future<void> main() async {
  final client = await WgClient.connect(NativeWingetBridge());

  print('Installed packages:');
  final tx = client.listInstalled();
  int count = 0;
  await for (final pkg in tx.packages) {
    print('  ${pkg.id.padRight(40)} ${pkg.version}');
    count++;
  }
  print('Total: $count packages');

  await client.close();
}
