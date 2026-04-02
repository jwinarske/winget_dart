import 'package:fluent_ui/fluent_ui.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../providers/packages_provider.dart';
import '../widgets/package_list.dart';

class InstalledScreen extends ConsumerWidget {
  const InstalledScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final installed = ref.watch(installedPackagesProvider);

    return ScaffoldPage.withPadding(
      header: PageHeader(
        title: const Text('Installed Packages'),
        commandBar: CommandBar(
          mainAxisAlignment: MainAxisAlignment.end,
          primaryItems: [
            CommandBarButton(
              icon: const Icon(FluentIcons.refresh),
              label: const Text('Refresh'),
              onPressed: () => ref.invalidate(installedPackagesProvider),
            ),
          ],
        ),
      ),
      content: installed.when(
        data:
            (pkgs) =>
                pkgs.isEmpty
                    ? Center(
                      child: Text(
                        'No installed packages found',
                        style: FluentTheme.of(context).typography.body,
                      ),
                    )
                    : PackageList(packages: pkgs, showUninstallButton: true),
        loading: () => const Center(child: ProgressRing()),
        error:
            (err, _) => Center(
              child: InfoBar(
                title: const Text('Failed to load installed packages'),
                content: Text('$err'),
                severity: InfoBarSeverity.error,
              ),
            ),
      ),
    );
  }
}
