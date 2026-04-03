/// Base class for all winget_dart exceptions.
class WgException implements Exception {
  /// Human-readable error description.
  final String message;

  /// The raw HRESULT from the Windows Package Manager COM server, if available.
  final int? hresult;

  /// Creates a [WgException] with a [message] and optional [hresult].
  const WgException(this.message, {this.hresult});

  /// The [hresult] formatted as an uppercase hex string (e.g. `0x80070005`),
  /// or `null` if no HRESULT was provided.
  String? get hresultHex => hresult != null
      ? '0x${hresult!.toUnsigned(32).toRadixString(16).toUpperCase()}'
      : null;

  @override
  String toString() {
    final hr = hresultHex;
    return hr != null ? 'WgException($hr): $message' : 'WgException: $message';
  }
}
