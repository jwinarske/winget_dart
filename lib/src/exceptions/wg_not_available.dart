import 'wg_exception.dart';

/// Thrown when the Windows Package Manager (App Installer) is not installed,
/// not registered, or not reachable on this machine.
class WgNotAvailableException extends WgException {
  /// Creates a [WgNotAvailableException] with the given [message].
  const WgNotAvailableException(super.message);

  @override
  String toString() => 'WgNotAvailableException: $message';
}
