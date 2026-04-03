import 'wg_exception.dart';

/// Thrown when an operation was cancelled via [WgClient.cancel].
class WgCancelledException extends WgException {
  /// Creates a [WgCancelledException].
  const WgCancelledException() : super('Operation was cancelled');

  @override
  String toString() => 'WgCancelledException';
}
