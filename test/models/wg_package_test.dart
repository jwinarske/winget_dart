import 'package:test/test.dart';
import 'package:winget_dart/winget_dart.dart';

void main() {
  group('WgPackage.fromJson', () {
    final base = {
      'id': 'Kitware.CMake',
      'name': 'CMake',
      'version': '3.28.0',
      'source': 'winget',
      'catalog': 'winget',
    };

    test('parses required fields', () {
      final p = WgPackage.fromJson(base);
      expect(p.id, equals('Kitware.CMake'));
      expect(p.name, equals('CMake'));
      expect(p.version, equals('3.28.0'));
      expect(p.source, equals('winget'));
      expect(p.catalogId, equals('winget'));
    });

    test('availableVersion is null when absent', () {
      final p = WgPackage.fromJson(base);
      expect(p.availableVersion, isNull);
    });

    test('parses availableVersion when present', () {
      final p = WgPackage.fromJson({...base, 'available_version': '3.29.0'});
      expect(p.availableVersion, equals('3.29.0'));
    });

    test('source defaults to empty string when null', () {
      final p = WgPackage.fromJson({...base, 'source': null});
      expect(p.source, equals(''));
    });

    test('catalogId defaults to empty string when null', () {
      final p = WgPackage.fromJson({...base, 'catalog': null});
      expect(p.catalogId, equals(''));
    });
  });
}
