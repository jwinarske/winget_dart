import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:winget_dart/winget_dart.dart';

import 'winget_provider.dart';

/// Installed packages list.
final installedPackagesProvider = FutureProvider<List<WgPackage>>((ref) async {
  final client = await ref.watch(wingetClientProvider.future);
  try {
    return await client.listInstalled().result;
  } on WgException catch (e) {
    throw Exception('Failed to list installed: ${e.message}');
  }
});

/// Packages with available updates.
final updatablePackagesProvider = FutureProvider<List<WgPackage>>((ref) async {
  // Wait for installed to finish first to avoid handle contention.
  await ref.watch(installedPackagesProvider.future);
  final client = await ref.watch(wingetClientProvider.future);
  try {
    return await client.getUpdates().result;
  } on WgException catch (e) {
    throw Exception('Failed to check updates: ${e.message}');
  }
});

/// Search results — refreshed when the query changes.
final searchQueryProvider = StateProvider<String>((ref) => '');

final searchResultsProvider = FutureProvider<List<WgPackage>>((ref) async {
  final query = ref.watch(searchQueryProvider);
  if (query.length < 2) return [];
  final client = await ref.watch(wingetClientProvider.future);
  try {
    return await client.searchName(query).result;
  } on WgException catch (e) {
    throw Exception('Search failed: ${e.message}');
  }
});

/// Available catalogs.
final catalogsProvider = FutureProvider<List<WgCatalog>>((ref) async {
  final client = await ref.watch(wingetClientProvider.future);
  return client.listCatalogs();
});
