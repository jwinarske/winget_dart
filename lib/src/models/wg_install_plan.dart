import 'wg_package.dart';

/// Result of a simulate (dry-run) operation showing what would change.
class WgInstallPlan {
  /// Packages that would be freshly installed.
  final List<WgPackage> installing;

  /// Packages that would be upgraded to a newer version.
  final List<WgPackage> upgrading;

  /// Packages that would be removed.
  final List<WgPackage> removing;

  /// Whether the plan has no changes.
  bool get isEmpty =>
      installing.isEmpty && upgrading.isEmpty && removing.isEmpty;

  /// Creates a [WgInstallPlan].
  const WgInstallPlan({
    required this.installing,
    required this.upgrading,
    required this.removing,
  });

  /// Deserializes a [WgInstallPlan] from a JSON map.
  factory WgInstallPlan.fromJson(Map<String, dynamic> j) => WgInstallPlan(
        installing: (j['installing'] as List? ?? [])
            .map((e) => WgPackage.fromJson(e as Map<String, dynamic>))
            .toList(),
        upgrading: (j['upgrading'] as List? ?? [])
            .map((e) => WgPackage.fromJson(e as Map<String, dynamic>))
            .toList(),
        removing: (j['removing'] as List? ?? [])
            .map((e) => WgPackage.fromJson(e as Map<String, dynamic>))
            .toList(),
      );
}
