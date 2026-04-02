import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:winget_dart/winget_dart.dart';
import 'package:winget_dart/src/bridge/native_winget_bridge.dart';

/// The singleton WgClient instance, connected on first access.
final wingetClientProvider = FutureProvider<WgClient>((ref) async {
  final client = await WgClient.connect(NativeWingetBridge());
  ref.onDispose(() => client.close());
  return client;
});
