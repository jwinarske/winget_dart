import 'wg_package.dart';

class WgInstallPlan {
  final List<WgPackage> installing;
  final List<WgPackage> upgrading;
  final List<WgPackage> removing;

  bool get isEmpty =>
      installing.isEmpty && upgrading.isEmpty && removing.isEmpty;

  const WgInstallPlan({
    required this.installing,
    required this.upgrading,
    required this.removing,
  });

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
