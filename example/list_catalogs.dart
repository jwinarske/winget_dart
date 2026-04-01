import 'package:winget_dart/winget_dart.dart';
import 'package:winget_dart/src/bridge/native_winget_bridge.dart';

Future<void> main() async {
  final client = await WgClient.connect(NativeWingetBridge());

  final catalogs = await client.listCatalogs();
  print('Configured package catalogs:');
  for (final c in catalogs) {
    print('  ${c.id.padRight(20)} ${c.name}');
  }

  await client.close();
}
