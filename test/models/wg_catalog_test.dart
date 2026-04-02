import 'package:test/test.dart';
import 'package:winget_dart/winget_dart.dart';

void main() {
  group('WgCatalog', () {
    test('fromJson parses id and name', () {
      final c = WgCatalog.fromJson({'id': 'winget', 'name': 'WinGet'});
      expect(c.id, equals('winget'));
      expect(c.name, equals('WinGet'));
    });

    test('toString includes id', () {
      final c = WgCatalog(id: 'msstore', name: 'Microsoft Store');
      expect(c.toString(), equals('WgCatalog(msstore)'));
    });
  });
}
