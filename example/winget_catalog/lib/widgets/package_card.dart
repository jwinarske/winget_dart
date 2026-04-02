import 'package:fluent_ui/fluent_ui.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:winget_dart/winget_dart.dart';

import '../providers/operation_provider.dart';

/// A single package row displayed as a Fluent UI card.
class PackageCard extends ConsumerWidget {
  final WgPackage package;
  final bool showInstall;
  final bool showUninstall;
  final bool showUpgrade;
  final bool showAvailableVersion;

  const PackageCard({
    super.key,
    required this.package,
    this.showInstall = false,
    this.showUninstall = false,
    this.showUpgrade = false,
    this.showAvailableVersion = false,
  });

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final theme = FluentTheme.of(context);
    final ops = ref.read(operationsProvider.notifier);

    return Card(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
      child: Row(
        children: [
          // Package icon placeholder
          Container(
            width: 40,
            height: 40,
            decoration: BoxDecoration(
              color: theme.accentColor.withValues(alpha: 0.1),
              borderRadius: BorderRadius.circular(8),
            ),
            child: Icon(
              FluentIcons.product,
              size: 20,
              color: theme.accentColor,
            ),
          ),
          const SizedBox(width: 16),

          // Package info
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  package.name,
                  style: theme.typography.bodyStrong,
                  overflow: TextOverflow.ellipsis,
                ),
                const SizedBox(height: 2),
                Text(
                  package.id,
                  style: theme.typography.caption?.copyWith(
                    color: theme.inactiveColor,
                  ),
                  overflow: TextOverflow.ellipsis,
                ),
              ],
            ),
          ),

          // Version info
          Column(
            crossAxisAlignment: CrossAxisAlignment.end,
            children: [
              Text(package.version, style: theme.typography.caption),
              if (showAvailableVersion && package.availableVersion != null) ...[
                const SizedBox(height: 2),
                Text(
                  '\u2192 ${package.availableVersion}',
                  style: theme.typography.caption?.copyWith(
                    color: theme.accentColor,
                    fontWeight: FontWeight.w600,
                  ),
                ),
              ],
            ],
          ),
          const SizedBox(width: 12),

          // Action buttons
          if (showInstall)
            FilledButton(
              onPressed: () => ops.install(package.id),
              child: const Text('Install'),
            ),
          if (showUpgrade)
            FilledButton(
              onPressed: () => ops.upgrade(package.id),
              child: const Text('Upgrade'),
            ),
          if (showUninstall)
            Button(
              onPressed: () => _confirmUninstall(context, ops),
              child: const Text('Uninstall'),
            ),
        ],
      ),
    );
  }

  void _confirmUninstall(BuildContext context, OperationsNotifier ops) async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder:
          (ctx) => ContentDialog(
            title: const Text('Uninstall package?'),
            content: Text('Remove ${package.name} (${package.id})?'),
            actions: [
              Button(
                onPressed: () => Navigator.pop(ctx, false),
                child: const Text('Cancel'),
              ),
              FilledButton(
                onPressed: () => Navigator.pop(ctx, true),
                child: const Text('Uninstall'),
              ),
            ],
          ),
    );
    if (confirmed == true) {
      ops.uninstall(package.id);
    }
  }
}
