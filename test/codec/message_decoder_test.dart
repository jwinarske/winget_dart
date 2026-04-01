import 'package:test/test.dart';
import 'package:winget_dart/src/codec/message_decoder.dart';

void main() {
  group('MessageDecoder.decode', () {
    test('decodes a valid JSON object', () {
      final result = MessageDecoder.decode('{"done":true}');
      expect(result, equals({'done': true}));
    });

    test('decodes nested objects', () {
      final result = MessageDecoder.decode(
          '{"pkg":{"id":"X","name":"Y","version":"1.0"}}');
      expect(result['pkg'], isA<Map<String, dynamic>>());
    });

    test('throws FormatException for a JSON array', () {
      expect(() => MessageDecoder.decode('[1,2,3]'), throwsFormatException);
    });

    test('throws FormatException for a JSON string', () {
      expect(() => MessageDecoder.decode('"hello"'), throwsFormatException);
    });

    test('throws FormatException for a JSON number', () {
      expect(() => MessageDecoder.decode('42'), throwsFormatException);
    });

    test('throws FormatException for invalid JSON', () {
      expect(() => MessageDecoder.decode('{bad json}'), throwsFormatException);
    });
  });

  group('MessageDecoder.discriminator', () {
    const knownKeys = [
      'pkg', 'catalog', 'progress', 'plan',
      'done', 'result', 'error', 'cancelled', 'ok',
    ];

    for (final key in knownKeys) {
      test('identifies "$key" discriminator', () {
        expect(MessageDecoder.discriminator({key: true}), equals(key));
      });
    }

    test('throws FormatException for an unknown discriminator', () {
      expect(
        () => MessageDecoder.discriminator({'unknown_key': 1}),
        throwsFormatException,
      );
    });

    test('throws FormatException for an empty map', () {
      expect(
        () => MessageDecoder.discriminator({}),
        throwsFormatException,
      );
    });

    test('returns a known key when multiple are present', () {
      final msg = {'pkg': {}, 'done': true};
      expect(knownKeys, contains(MessageDecoder.discriminator(msg)));
    });
  });
}
