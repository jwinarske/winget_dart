import 'package:test/test.dart';
import 'package:winget_dart/winget_dart.dart';

void main() {
  group('WgException', () {
    test('toString includes message without hresult', () {
      const e = WgException('Something went wrong');
      expect(e.toString(), contains('Something went wrong'));
      expect(e.toString(), isNot(contains('0x')));
    });

    test('toString includes hex hresult when present', () {
      const e = WgException('Access denied', hresult: -2147024891);
      expect(e.toString(), contains('0x80070005'));
    });

    test('hresultHex formats 32-bit unsigned hex', () {
      const e = WgException('', hresult: -2147024891);
      expect(e.hresultHex, equals('0x80070005'));
    });

    test('hresultHex is null when hresult is null', () {
      const e = WgException('No hresult');
      expect(e.hresultHex, isNull);
    });

    test('hresultHex is uppercase hex', () {
      const e = WgException('', hresult: -2147024894);
      expect(e.hresultHex, equals('0x80070002'));
    });

    test('is an Exception', () {
      expect(const WgException('x'), isA<Exception>());
    });
  });

  group('WgNotAvailableException', () {
    test('toString starts with class name', () {
      const e = WgNotAvailableException('WinGet not found');
      expect(e.toString(), startsWith('WgNotAvailableException'));
      expect(e.toString(), contains('WinGet not found'));
    });

    test('is a WgException', () {
      expect(const WgNotAvailableException('x'), isA<WgException>());
    });
  });

  group('WgElevationErrorException', () {
    test('toString includes message', () {
      const e = WgElevationErrorException('Wrong factory');
      expect(e.toString(), contains('Wrong factory'));
    });

    test('carries hresult', () {
      const e = WgElevationErrorException('Elevation error', hresult: -1);
      expect(e.hresult, equals(-1));
    });
  });

  group('WgCancelledException', () {
    test('toString is WgCancelledException', () {
      const e = WgCancelledException();
      expect(e.toString(), equals('WgCancelledException'));
    });

    test('has a fixed message', () {
      const e = WgCancelledException();
      expect(e.message, equals('Operation was cancelled'));
    });

    test('is a WgException', () {
      expect(const WgCancelledException(), isA<WgException>());
    });
  });
}
