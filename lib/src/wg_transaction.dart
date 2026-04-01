import 'models/wg_package.dart';
import 'models/wg_progress.dart';

/// Wraps the result of an asynchronous WinGet operation.
///
/// For streaming operations (search, list), [packages] emits results as they
/// arrive and [result] completes with the full list when done.
///
/// For progress operations (install, upgrade, uninstall), [progress] emits
/// events during execution and [result] completes when the operation finishes.
class WgTransaction<T> {
  /// Stream of [WgPackage] results (search / list operations).
  final Stream<WgPackage> packages;

  /// Stream of [WgProgress] events (install / upgrade / uninstall).
  final Stream<WgProgress> progress;

  /// Completes when the operation finishes. Throws [WgException] on failure.
  final Future<T> result;

  const WgTransaction({
    required this.packages,
    required this.progress,
    required this.result,
  });
}
