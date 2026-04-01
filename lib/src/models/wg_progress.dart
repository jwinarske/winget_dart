enum WgInstallState {
  queued,
  downloading,
  installing,
  postInstall,
  finished,
  unknown;

  static WgInstallState from(String s) => WgInstallState.values.firstWhere(
        (e) => e.name == s,
        orElse: () => WgInstallState.unknown,
      );
}

class WgProgress {
  final int percent;
  final WgInstallState state;
  final String label;

  const WgProgress({
    required this.percent,
    required this.state,
    required this.label,
  });

  factory WgProgress.fromJson(Map<String, dynamic> j) => WgProgress(
        percent: j['percent'] as int? ?? 0,
        state: WgInstallState.from(j['state'] as String? ?? 'unknown'),
        label: j['label'] as String? ?? '',
      );

  @override
  String toString() => 'WgProgress($percent% $state)';
}
