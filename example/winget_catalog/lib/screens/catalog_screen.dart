import 'package:fluent_ui/fluent_ui.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../providers/packages_provider.dart';
import '../widgets/package_list.dart';
import '../widgets/search_box.dart';

class CatalogScreen extends ConsumerWidget {
  const CatalogScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final results = ref.watch(searchResultsProvider);
    final query = ref.watch(searchQueryProvider);

    return ScaffoldPage.withPadding(
      header: PageHeader(
        title: const Text('Catalog'),
        commandBar: SizedBox(
          width: 320,
          child: WingetSearchBox(
            onChanged: (q) => ref.read(searchQueryProvider.notifier).state = q,
          ),
        ),
      ),
      content:
          query.length < 2
              ? Center(
                child: Column(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    Icon(
                      FluentIcons.search,
                      size: 48,
                      color: FluentTheme.of(
                        context,
                      ).inactiveColor.withValues(alpha: 0.3),
                    ),
                    const SizedBox(height: 16),
                    Text(
                      'Search the WinGet catalog',
                      style: FluentTheme.of(context).typography.subtitle,
                    ),
                    const SizedBox(height: 8),
                    Text(
                      'Type at least 2 characters to search',
                      style: FluentTheme.of(context).typography.body?.copyWith(
                        color: FluentTheme.of(context).inactiveColor,
                      ),
                    ),
                  ],
                ),
              )
              : results.when(
                data:
                    (pkgs) =>
                        pkgs.isEmpty
                            ? Center(
                              child: Text(
                                'No packages found for "$query"',
                                style: FluentTheme.of(context).typography.body,
                              ),
                            )
                            : PackageList(
                              packages: pkgs,
                              showInstallButton: true,
                            ),
                loading: () => const Center(child: ProgressRing()),
                error:
                    (err, _) => Center(
                      child: InfoBar(
                        title: const Text('Search failed'),
                        content: Text('$err'),
                        severity: InfoBarSeverity.error,
                      ),
                    ),
              ),
    );
  }
}
