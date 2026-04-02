import 'package:test/test.dart';
import 'package:winget_dart/winget_dart.dart';

void main() {
  group('WgInstallState.from', () {
    const cases = {
      'queued': WgInstallState.queued,
      'downloading': WgInstallState.downloading,
      'installing': WgInstallState.installing,
      'postInstall': WgInstallState.postInstall,
      'finished': WgInstallState.finished,
    };

    cases.forEach((name, expected) {
      test('maps "$name" to ${expected.name}', () {
        expect(WgInstallState.from(name), equals(expected));
      });
    });

    test('maps unknown string to unknown', () {
      expect(WgInstallState.from(''), equals(WgInstallState.unknown));
      expect(WgInstallState.from('garbage'), equals(WgInstallState.unknown));
      expect(
          WgInstallState.from('DOWNLOADING'), equals(WgInstallState.unknown));
    });
  });

  group('WgProgress.fromJson', () {
    test('parses all fields', () {
      final p = WgProgress.fromJson(
          {'percent': 42, 'state': 'installing', 'label': 'Installing...'});
      expect(p.percent, equals(42));
      expect(p.state, equals(WgInstallState.installing));
      expect(p.label, equals('Installing...'));
    });

    test('defaults when fields are absent', () {
      final p = WgProgress.fromJson({});
      expect(p.percent, equals(0));
      expect(p.state, equals(WgInstallState.unknown));
      expect(p.label, equals(''));
    });

    test('defaults when state is null', () {
      final p = WgProgress.fromJson({'percent': 10, 'state': null});
      expect(p.state, equals(WgInstallState.unknown));
    });
  });
}
