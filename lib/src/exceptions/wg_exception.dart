/// Base class for all winget_dart exceptions.
class WgException implements Exception {
  final String message;

  /// The raw HRESULT from the Windows Package Manager COM server, if available.
  final int? hresult;

  const WgException(this.message, {this.hresult});

  String? get hresultHex => hresult != null
      ? '0x${hresult!.toUnsigned(32).toRadixString(16).toUpperCase()}'
      : null;

  @override
  String toString() {
    final hr = hresultHex;
    return hr != null ? 'WgException($hr): $message' : 'WgException: $message';
  }
}
