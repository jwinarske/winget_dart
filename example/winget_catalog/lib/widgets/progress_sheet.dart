import 'package:fluent_ui/fluent_ui.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../models/view_models.dart';
import '../providers/operation_provider.dart';

/// Activity screen showing all running/completed operations.
class ProgressSheet extends ConsumerWidget {
  const ProgressSheet({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final operations = ref.watch(operationsProvider);
    final theme = FluentTheme.of(context);

    return ScaffoldPage.withPadding(
      header: PageHeader(
        title: const Text('Activity'),
        commandBar: CommandBar(
          mainAxisAlignment: MainAxisAlignment.end,
          primaryItems: [
            CommandBarButton(
              icon: const Icon(FluentIcons.clear),
              label: const Text('Clear Finished'),
              onPressed:
                  () => ref.read(operationsProvider.notifier).clearFinished(),
            ),
          ],
        ),
      ),
      content:
          operations.isEmpty
              ? Center(
                child: Column(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    Icon(
                      FluentIcons.history,
                      size: 48,
                      color: theme.inactiveColor.withValues(alpha: 0.3),
                    ),
                    const SizedBox(height: 16),
                    Text(
                      'No recent activity',
                      style: theme.typography.subtitle,
                    ),
                  ],
                ),
              )
              : ListView.builder(
                itemCount: operations.length,
                itemBuilder: (context, i) {
                  final op = operations[operations.length - 1 - i];
                  return _OperationTile(op: op);
                },
              ),
    );
  }
}

class _OperationTile extends StatelessWidget {
  final WingetOperation op;

  const _OperationTile({required this.op});

  @override
  Widget build(BuildContext context) {
    final theme = FluentTheme.of(context);

    final (IconData icon, Color color) = switch (op.status) {
      OperationStatus.running => (FluentIcons.sync, theme.accentColor),
      OperationStatus.completed => (FluentIcons.completed, Colors.green),
      OperationStatus.failed => (FluentIcons.error_badge, Colors.red),
      OperationStatus.pending => (FluentIcons.clock, theme.inactiveColor),
    };

    return Card(
      margin: const EdgeInsets.only(bottom: 4),
      padding: const EdgeInsets.all(12),
      child: Row(
        children: [
          Icon(icon, color: color, size: 20),
          const SizedBox(width: 12),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  '${op.type.label} ${op.packageId}',
                  style: theme.typography.bodyStrong,
                ),
                if (op.label.isNotEmpty) ...[
                  const SizedBox(height: 4),
                  Text(op.label, style: theme.typography.caption),
                ],
                if (op.error != null) ...[
                  const SizedBox(height: 4),
                  Text(
                    op.error!,
                    style: theme.typography.caption?.copyWith(
                      color: Colors.red,
                    ),
                  ),
                ],
              ],
            ),
          ),
          if (op.status == OperationStatus.running) ...[
            SizedBox(
              width: 80,
              child: ProgressBar(value: op.percent.toDouble()),
            ),
            const SizedBox(width: 8),
            Text('${op.percent}%', style: theme.typography.caption),
          ],
        ],
      ),
    );
  }
}
