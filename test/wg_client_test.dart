import 'package:test/test.dart';
import 'package:winget_dart/winget_dart.dart';
import 'helpers/fake_winget_bridge.dart';

void main() {
  group('WgClient.connect', () {
    test('connects successfully with default fake', () async {
      final fake = FakeWingetBridge();
      final client = await WgClient.connect(fake);
      addTearDown(client.close);
      expect(client, isNotNull);
    });

    test('throws WgNotAvailableException when unavailable', () {
      final fake = FakeWingetBridge()..stubIsAvailable(false);
      expect(WgClient.connect(fake), throwsA(isA<WgNotAvailableException>()));
    });

    test('throws WgException on connect error', () {
      final fake = FakeWingetBridge()
        ..stubConnect(ok: false, errorMessage: 'COM failure', hresult: -1);
      expect(WgClient.connect(fake), throwsA(isA<WgException>()));
    });
  });

  group('WgClient.listCatalogs', () {
    test('returns catalogs from bridge', () async {
      final fake = FakeWingetBridge()
        ..stubCatalogs([
          Msg.catalog('winget', name: 'WinGet Community Repository'),
          Msg.catalog('msstore', name: 'Microsoft Store'),
          Msg.done,
        ]);
      final client = await WgClient.connect(fake);
      addTearDown(client.close);

      final catalogs = await client.listCatalogs();
      expect(catalogs, hasLength(2));
      expect(catalogs[0].id, equals('winget'));
      expect(catalogs[1].id, equals('msstore'));
    });

    test('returns empty list when no catalogs', () async {
      final fake = FakeWingetBridge()..stubCatalogs([Msg.done]);
      final client = await WgClient.connect(fake);
      addTearDown(client.close);
      expect(await client.listCatalogs(), isEmpty);
    });
  });

  group('WgClient.searchName', () {
    test('returns packages from search', () async {
      final fake = FakeWingetBridge()
        ..stubSearchName('cmake', [
          Msg.pkg(id: 'Kitware.CMake', name: 'CMake', version: '3.28.0'),
          Msg.done,
        ]);
      final client = await WgClient.connect(fake);
      addTearDown(client.close);

      final packages = await client.searchName('cmake').result;
      expect(packages, hasLength(1));
      expect(packages.first.id, equals('Kitware.CMake'));
    });
  });

  group('WgClient.findById', () {
    test('returns package when found', () async {
      final fake = FakeWingetBridge()
        ..stubFindById(
            'Kitware.CMake', Msg.pkg(id: 'Kitware.CMake', name: 'CMake'));
      final client = await WgClient.connect(fake);
      addTearDown(client.close);

      final pkg = await client.findById('Kitware.CMake');
      expect(pkg, isNotNull);
      expect(pkg!.id, equals('Kitware.CMake'));
    });

    test('throws on error', () async {
      final fake = FakeWingetBridge()
        ..stubFindById('Nope', Msg.error('Not found'));
      final client = await WgClient.connect(fake);
      addTearDown(client.close);

      expect(client.findById('Nope'), throwsA(isA<WgException>()));
    });
  });

  group('WgClient.simulateInstall', () {
    test('returns install plan', () async {
      final fake = FakeWingetBridge()
        ..stubSimulateInstall(
            'Kitware.CMake',
            Msg.plan(installing: [
              {'id': 'Kitware.CMake', 'name': 'CMake', 'version': '3.28.0'}
            ]));
      final client = await WgClient.connect(fake);
      addTearDown(client.close);

      final plan = await client.simulateInstall('Kitware.CMake');
      expect(plan.installing, hasLength(1));
      expect(plan.isEmpty, isFalse);
    });

    test('returns empty plan', () async {
      final fake = FakeWingetBridge()
        ..stubSimulateInstall('Already.Installed', Msg.plan());
      final client = await WgClient.connect(fake);
      addTearDown(client.close);

      final plan = await client.simulateInstall('Already.Installed');
      expect(plan.isEmpty, isTrue);
    });
  });

  group('WgClient.installPackage', () {
    test('completes successfully with progress', () async {
      final fake = FakeWingetBridge()
        ..stubInstall('Pkg.A', [
          Msg.progress(25),
          Msg.progress(75, state: 'installing'),
          Msg.success,
        ]);
      final client = await WgClient.connect(fake);
      addTearDown(client.close);

      final tx = client.installPackage('Pkg.A');
      final percents = <int>[];
      await for (final p in tx.progress) {
        percents.add(p.percent);
      }
      await tx.result;
      expect(percents, equals([25, 75]));
    });
  });

  group('WgClient.upgradePackage', () {
    test('completes successfully', () async {
      final fake = FakeWingetBridge()..stubUpgrade('Pkg.A', [Msg.success]);
      final client = await WgClient.connect(fake);
      addTearDown(client.close);

      await client.upgradePackage('Pkg.A').result;
    });
  });

  group('WgClient.uninstallPackage', () {
    test('completes successfully', () async {
      final fake = FakeWingetBridge()..stubUninstall('Pkg.A', [Msg.success]);
      final client = await WgClient.connect(fake);
      addTearDown(client.close);

      await client.uninstallPackage('Pkg.A').result;
    });
  });

  group('WgClient.listInstalled', () {
    test('returns installed packages', () async {
      final fake = FakeWingetBridge()
        ..stubListInstalled([
          Msg.pkg(id: 'Pkg.A', name: 'A', source: 'local'),
          Msg.done,
        ]);
      final client = await WgClient.connect(fake);
      addTearDown(client.close);

      final packages = await client.listInstalled().result;
      expect(packages, hasLength(1));
      expect(packages.first.source, equals('local'));
    });
  });

  group('WgClient.getUpdates', () {
    test('returns packages with updates', () async {
      final fake = FakeWingetBridge()
        ..stubGetUpdates([
          Msg.pkg(
              id: 'Pkg.A',
              name: 'A',
              version: '1.0.0',
              availableVersion: '2.0.0'),
          Msg.done,
        ]);
      final client = await WgClient.connect(fake);
      addTearDown(client.close);

      final updates = await client.getUpdates().result;
      expect(updates, hasLength(1));
      expect(updates.first.availableVersion, equals('2.0.0'));
    });
  });
}
