import 'wg_exception.dart';

/// Thrown when the wrong COM factory was selected for the current elevation
/// level. Should not occur in normal usage.
class WgElevationErrorException extends WgException {
  /// Creates a [WgElevationErrorException] with the given [message] and
  /// optional [hresult].
  const WgElevationErrorException(super.message, {super.hresult});

  @override
  String toString() => 'WgElevationErrorException: $message';
}
