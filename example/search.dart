import 'package:winget_dart/winget_dart.dart';
import 'package:winget_dart/src/bridge/native_winget_bridge.dart';

Future<void> main(List<String> args) async {
  final query = args.isNotEmpty ? args[0] : 'cmake';
  final client = await WgClient.connect(NativeWingetBridge());

  print('Searching for "$query"...');
  final tx = client.searchName(query);
  await for (final pkg in tx.packages) {
    print('  ${pkg.id.padRight(40)} ${pkg.version}');
  }

  await client.close();
}
