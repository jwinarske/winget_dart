import 'package:fluent_ui/fluent_ui.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:system_theme/system_theme.dart';

import 'screens/shell.dart';
import 'theme/app_theme.dart';

class WingetCatalogApp extends ConsumerWidget {
  const WingetCatalogApp({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final accent = SystemTheme.accentColor.accent;
    return FluentApp(
      title: 'WinGet Catalog',
      theme: AppTheme.light(accent),
      darkTheme: AppTheme.dark(accent),
      themeMode: ThemeMode.system,
      debugShowCheckedModeBanner: false,
      home: const AppShell(),
    );
  }
}
