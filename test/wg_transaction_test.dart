import 'dart:async';
import 'package:test/test.dart';
import 'package:winget_dart/winget_dart.dart';
import 'helpers/fake_winget_bridge.dart';

void main() {
  group('WgTransaction — streaming operations', () {
    late WgClient client;
    late FakeWingetBridge fake;

    setUp(() async {
      fake = FakeWingetBridge();
      client = await WgClient.connectWith(fake);
    });

    tearDown(() => client.close());

    test('packages stream emits items in order then closes', () async {
      fake.stubSearchName('q', [
        Msg.pkg(id: 'A', name: 'Alpha'),
        Msg.pkg(id: 'B', name: 'Beta'),
        Msg.pkg(id: 'C', name: 'Gamma'),
        Msg.done,
      ]);

      final tx = client.searchName('q');
      final ids = <String>[];
      await for (final p in tx.packages) {
        ids.add(p.id);
      }

      expect(ids, equals(['A', 'B', 'C']));
    });

    test('result future resolves after packages stream closes', () async {
      fake.stubSearchName('q', [
        Msg.pkg(id: 'X', name: 'X'),
        Msg.done,
      ]);
      final tx = client.searchName('q');
      final result = await tx.result;
      expect(result, hasLength(1));
      expect(result.single.id, equals('X'));
    });

    test('packages stream and result future contain the same items', () async {
      fake.stubSearchName('q', [
        Msg.pkg(id: 'A', name: 'A'),
        Msg.pkg(id: 'B', name: 'B'),
        Msg.done,
      ]);
      final tx = client.searchName('q');
      final streamItems = <WgPackage>[];
      await for (final p in tx.packages) {
        streamItems.add(p);
      }
      final resultItems = await tx.result;
      expect(streamItems.map((p) => p.id),
          equals(resultItems.map((p) => p.id)));
    });

    test('error from bridge propagates to packages stream', () async {
      fake.stubSearchName('bad', [
        Msg.error('Catalog unavailable', hresult: -1),
      ]);
      final tx = client.searchName('bad');
      expect(tx.packages.toList(), throwsA(isA<WgException>()));
    });

    test('error from bridge propagates to result future', () async {
      fake.stubSearchName('bad', [
        Msg.error('Catalog unavailable', hresult: -1),
      ]);
      final tx = client.searchName('bad');
      unawaited(tx.packages.toList().catchError((_) => <WgPackage>[]));
      expect(tx.result, throwsA(isA<WgException>()));
    });

    test('progress stream is empty for streaming operations', () async {
      fake.stubSearchName('q', [Msg.done]);
      final tx = client.searchName('q');
      await tx.result;
      expect(await tx.progress.toList(), isEmpty);
    });
  });

  group('WgTransaction — progress operations', () {
    late WgClient client;
    late FakeWingetBridge fake;

    setUp(() async {
      fake = FakeWingetBridge();
      client = await WgClient.connectWith(fake);
    });

    tearDown(() => client.close());

    test('progress stream emits events in order then closes', () async {
      fake.stubInstall('Pkg.A', [
        Msg.progress(10),
        Msg.progress(50, state: 'installing'),
        Msg.progress(90, state: 'installing'),
        Msg.success,
      ]);

      final tx = client.installPackage('Pkg.A');
      final percents = <int>[];
      await for (final p in tx.progress) {
        percents.add(p.percent);
      }
      await tx.result;

      expect(percents, equals([10, 50, 90]));
    });

    test('result future resolves after progress stream closes', () async {
      fake.stubInstall('Pkg.A',
          [Msg.progress(100, state: 'finished'), Msg.success]);
      final tx = client.installPackage('Pkg.A');
      await for (final _ in tx.progress) {}
      expect(await tx.result, isNull);
    });

    test('packages stream is empty for progress operations', () async {
      fake.stubInstall('Pkg.A', [Msg.success]);
      final tx = client.installPackage('Pkg.A');
      await tx.result;
      expect(await tx.packages.toList(), isEmpty);
    });

    test('cancellation propagates as WgCancelledException', () async {
      fake.stubInstall('Pkg.A', [
        Msg.progress(25),
        Msg.cancelled,
      ]);
      final tx = client.installPackage('Pkg.A');
      unawaited(tx.progress.toList().catchError((_) => <WgProgress>[]));
      expect(tx.result, throwsA(isA<WgCancelledException>()));
    });

    test('error during install propagates to result', () async {
      fake.stubInstall('Pkg.A', [
        Msg.progress(30),
        Msg.error('Installation failed', hresult: -2147024891),
      ]);
      final tx = client.installPackage('Pkg.A');
      unawaited(tx.progress.toList().catchError((_) => <WgProgress>[]));
      expect(
          tx.result,
          throwsA(isA<WgException>()
              .having((e) => e.hresult, 'hresult', -2147024891)));
    });
  });
}
