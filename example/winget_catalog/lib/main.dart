import 'dart:io';

import 'package:fluent_ui/fluent_ui.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:system_theme/system_theme.dart';

import 'app.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await SystemTheme.accentColor;

  // Log uncaught Flutter errors to stderr for debugging.
  FlutterError.onError = (details) {
    FlutterError.presentError(details);
    stderr.writeln('FlutterError: ${details.exception}');
  };

  runApp(const ProviderScope(child: WingetCatalogApp()));
}
