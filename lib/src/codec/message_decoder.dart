import 'dart:convert';
import 'dart:typed_data';

/// Decodes the JSON messages posted from winget_nc.dll via Dart_PostCObject_DL.
///
/// Every message is a single JSON object with exactly one discriminator key:
///   pkg, catalog, progress, plan, done, result, error, cancelled, ok.
///
/// Messages arrive as Uint8List (typed data) containing UTF-8 JSON.
abstract final class MessageDecoder {
  /// Convert a raw message from the native port to a JSON string.
  /// Handles both Uint8List (typed data) and String (legacy) formats.
  static String messageToString(Object msg) {
    if (msg is Uint8List) return utf8.decode(msg);
    if (msg is String) return msg;
    throw FormatException(
        'winget_dart: unexpected message type: ${msg.runtimeType}');
  }

  /// Decode a raw message (String or Uint8List) from the bridge into a map.
  static Map<String, dynamic> decode(Object raw) {
    final str = messageToString(raw);
    final dynamic parsed = json.decode(str);
    if (parsed is! Map<String, dynamic>) {
      throw FormatException('winget_dart: bridge posted non-object JSON: $str');
    }
    return parsed;
  }

  /// Returns the discriminator key for a decoded message.
  static String discriminator(Map<String, dynamic> msg) {
    const keys = {
      'pkg',
      'catalog',
      'progress',
      'plan',
      'done',
      'result',
      'error',
      'cancelled',
      'ok',
    };
    for (final k in keys) {
      if (msg.containsKey(k)) return k;
    }
    throw FormatException('winget_dart: unknown message shape: $msg');
  }
}
