import 'package:fluent_ui/fluent_ui.dart';
import 'package:winget_dart/winget_dart.dart';

import 'package_card.dart';

/// Scrollable list of package cards.
class PackageList extends StatelessWidget {
  final List<WgPackage> packages;
  final bool showInstallButton;
  final bool showUninstallButton;
  final bool showUpgradeButton;
  final bool showAvailableVersion;

  const PackageList({
    super.key,
    required this.packages,
    this.showInstallButton = false,
    this.showUninstallButton = false,
    this.showUpgradeButton = false,
    this.showAvailableVersion = false,
  });

  @override
  Widget build(BuildContext context) {
    return ListView.builder(
      padding: const EdgeInsets.only(bottom: 24),
      itemCount: packages.length,
      itemBuilder:
          (context, i) => Padding(
            padding: const EdgeInsets.only(bottom: 4),
            child: PackageCard(
              package: packages[i],
              showInstall: showInstallButton,
              showUninstall: showUninstallButton,
              showUpgrade: showUpgradeButton,
              showAvailableVersion: showAvailableVersion,
            ),
          ),
    );
  }
}
