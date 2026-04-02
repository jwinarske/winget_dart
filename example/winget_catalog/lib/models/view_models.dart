/// Sort options for package lists.
enum PackageSort { name, id }

/// Type of winget operation.
enum OperationType {
  install('Installing'),
  uninstall('Uninstalling'),
  upgrade('Upgrading');

  final String label;
  const OperationType(this.label);
}

/// Status of a running operation.
enum OperationStatus { pending, running, completed, failed }

/// A tracked winget operation with live progress.
class WingetOperation {
  final String id;
  final String packageId;
  final OperationType type;
  final OperationStatus status;
  final int percent;
  final String label;
  final String? error;

  const WingetOperation({
    required this.id,
    required this.packageId,
    required this.type,
    this.status = OperationStatus.pending,
    this.percent = 0,
    this.label = '',
    this.error,
  });

  WingetOperation copyWith({
    OperationStatus? status,
    int? percent,
    String? label,
    String? error,
  }) => WingetOperation(
    id: id,
    packageId: packageId,
    type: type,
    status: status ?? this.status,
    percent: percent ?? this.percent,
    label: label ?? this.label,
    error: error ?? this.error,
  );
}
