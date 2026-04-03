/// The phase of an install, upgrade, or uninstall operation.
enum WgInstallState {
  /// Waiting in the WinGet queue.
  queued,

  /// Downloading package payload.
  downloading,

  /// Running the installer.
  installing,

  /// Post-install cleanup (e.g. PATH registration).
  postInstall,

  /// Operation completed successfully.
  finished,

  /// State not recognized by this client version.
  unknown;

  /// Parses a state name string, falling back to [unknown].
  static WgInstallState from(String s) => WgInstallState.values.firstWhere(
        (e) => e.name == s,
        orElse: () => WgInstallState.unknown,
      );
}

/// Progress event emitted during install, upgrade, or uninstall operations.
class WgProgress {
  /// Completion percentage (0–100).
  final int percent;

  /// Current phase of the operation.
  final WgInstallState state;

  /// Human-readable status label from WinGet.
  final String label;

  /// Creates a [WgProgress].
  const WgProgress({
    required this.percent,
    required this.state,
    required this.label,
  });

  /// Deserializes a [WgProgress] from a JSON map.
  factory WgProgress.fromJson(Map<String, dynamic> j) => WgProgress(
        percent: j['percent'] as int? ?? 0,
        state: WgInstallState.from(j['state'] as String? ?? 'unknown'),
        label: j['label'] as String? ?? '',
      );

  @override
  String toString() => 'WgProgress($percent% $state)';
}
