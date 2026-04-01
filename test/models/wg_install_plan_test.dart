import 'package:test/test.dart';
import 'package:winget_dart/winget_dart.dart';

void main() {
  final pkgJson = {
    'id': 'Pkg.A',
    'name': 'A',
    'version': '1.0.0',
    'source': 'winget',
    'catalog': 'winget',
  };

  group('WgInstallPlan.fromJson', () {
    test('all lists empty -> isEmpty is true', () {
      final plan = WgInstallPlan.fromJson(
          {'installing': [], 'upgrading': [], 'removing': []});
      expect(plan.isEmpty, isTrue);
    });

    test('non-empty installing -> isEmpty is false', () {
      final plan = WgInstallPlan.fromJson({
        'installing': [pkgJson],
        'upgrading': [],
        'removing': [],
      });
      expect(plan.isEmpty, isFalse);
      expect(plan.installing, hasLength(1));
    });

    test('parses all three lists independently', () {
      final plan = WgInstallPlan.fromJson({
        'installing': [pkgJson],
        'upgrading': [pkgJson],
        'removing': [pkgJson],
      });
      expect(plan.installing, hasLength(1));
      expect(plan.upgrading, hasLength(1));
      expect(plan.removing, hasLength(1));
    });

    test('missing lists default to empty', () {
      final plan = WgInstallPlan.fromJson({});
      expect(plan.installing, isEmpty);
      expect(plan.upgrading, isEmpty);
      expect(plan.removing, isEmpty);
    });
  });
}
