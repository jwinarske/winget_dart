import 'package:fluent_ui/fluent_ui.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../providers/operation_provider.dart';
import '../providers/packages_provider.dart';
import '../widgets/package_list.dart';

class UpdatesScreen extends ConsumerWidget {
  const UpdatesScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final updates = ref.watch(updatablePackagesProvider);

    return ScaffoldPage.withPadding(
      header: PageHeader(
        title: const Text('Available Updates'),
        commandBar: CommandBar(
          mainAxisAlignment: MainAxisAlignment.end,
          primaryItems: [
            CommandBarButton(
              icon: const Icon(FluentIcons.double_chevron_up),
              label: const Text('Upgrade All'),
              onPressed: () {
                final pkgs = updates.valueOrNull ?? [];
                final ops = ref.read(operationsProvider.notifier);
                for (final pkg in pkgs) {
                  ops.upgrade(pkg.id);
                }
              },
            ),
            CommandBarButton(
              icon: const Icon(FluentIcons.refresh),
              label: const Text('Refresh'),
              onPressed: () => ref.invalidate(updatablePackagesProvider),
            ),
          ],
        ),
      ),
      content: updates.when(
        data:
            (pkgs) =>
                pkgs.isEmpty
                    ? Center(
                      child: Column(
                        mainAxisSize: MainAxisSize.min,
                        children: [
                          Icon(
                            FluentIcons.completed,
                            size: 48,
                            color: FluentTheme.of(
                              context,
                            ).accentColor.withValues(alpha: 0.5),
                          ),
                          const SizedBox(height: 16),
                          Text(
                            'Everything is up to date',
                            style: FluentTheme.of(context).typography.subtitle,
                          ),
                        ],
                      ),
                    )
                    : PackageList(
                      packages: pkgs,
                      showUpgradeButton: true,
                      showAvailableVersion: true,
                    ),
        loading: () => const Center(child: ProgressRing()),
        error:
            (err, _) => Center(
              child: InfoBar(
                title: const Text('Failed to check for updates'),
                content: Text('$err'),
                severity: InfoBarSeverity.error,
              ),
            ),
      ),
    );
  }
}
