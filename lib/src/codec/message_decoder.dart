import 'dart:convert';

/// Decodes the JSON strings posted from winget_nc.dll via Dart_PostCObject_DL.
///
/// Every message is a single JSON object with exactly one discriminator key:
///   pkg, catalog, progress, plan, done, result, error, cancelled, ok.
abstract final class MessageDecoder {
  /// Decode a raw JSON string from the bridge into a typed map.
  static Map<String, dynamic> decode(String raw) {
    final dynamic parsed = json.decode(raw);
    if (parsed is! Map<String, dynamic>) {
      throw FormatException(
          'winget_dart: bridge posted non-object JSON: $raw');
    }
    return parsed;
  }

  /// Returns the discriminator key for a decoded message.
  static String discriminator(Map<String, dynamic> msg) {
    const keys = {
      'pkg', 'catalog', 'progress', 'plan',
      'done', 'result', 'error', 'cancelled', 'ok',
    };
    for (final k in keys) {
      if (msg.containsKey(k)) return k;
    }
    throw FormatException('winget_dart: unknown message shape: $msg');
  }
}
