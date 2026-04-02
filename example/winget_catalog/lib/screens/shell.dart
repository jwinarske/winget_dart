import 'package:fluent_ui/fluent_ui.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../models/view_models.dart';
import '../providers/operation_provider.dart';
import '../widgets/progress_sheet.dart';
import 'catalog_screen.dart';
import 'installed_screen.dart';
import 'updates_screen.dart';

class AppShell extends ConsumerStatefulWidget {
  const AppShell({super.key});

  @override
  ConsumerState<AppShell> createState() => _AppShellState();
}

class _AppShellState extends ConsumerState<AppShell> {
  int _selectedIndex = 0;

  @override
  Widget build(BuildContext context) {
    final operations = ref.watch(operationsProvider);
    final hasRunning = operations.any(
      (op) => op.status == OperationStatus.running,
    );

    return NavigationView(
      pane: NavigationPane(
        selected: _selectedIndex,
        onChanged: (i) => setState(() => _selectedIndex = i),
        displayMode: PaneDisplayMode.compact,
        header: Padding(
          padding: const EdgeInsets.only(left: 12, top: 8),
          child: Text(
            'WinGet Catalog',
            style: FluentTheme.of(context).typography.subtitle,
          ),
        ),
        items: [
          PaneItem(
            icon: const Icon(FluentIcons.search),
            title: const Text('Catalog'),
            body: const CatalogScreen(),
          ),
          PaneItem(
            icon: const Icon(FluentIcons.package),
            title: const Text('Installed'),
            body: const InstalledScreen(),
          ),
          PaneItem(
            icon: const Icon(FluentIcons.sync),
            title: const Text('Updates'),
            body: const UpdatesScreen(),
          ),
        ],
        footerItems: [
          PaneItem(
            icon: Icon(
              FluentIcons.progress_ring_dots,
              color: hasRunning ? FluentTheme.of(context).accentColor : null,
            ),
            title: const Text('Activity'),
            body: const ProgressSheet(),
          ),
        ],
      ),
    );
  }
}
