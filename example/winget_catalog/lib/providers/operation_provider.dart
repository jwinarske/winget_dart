import 'package:flutter/foundation.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:winget_dart/winget_dart.dart';

import '../models/view_models.dart';
import 'packages_provider.dart';
import 'winget_provider.dart';

final operationsProvider =
    StateNotifierProvider<OperationsNotifier, List<WingetOperation>>(
      (ref) => OperationsNotifier(ref),
    );

/// Manages winget install/upgrade/uninstall operations with progress.
class OperationsNotifier extends StateNotifier<List<WingetOperation>> {
  final Ref _ref;

  OperationsNotifier(this._ref) : super([]);

  Future<void> install(String packageId) async {
    final op = WingetOperation(
      id: UniqueKey().toString(),
      packageId: packageId,
      type: OperationType.install,
      status: OperationStatus.running,
    );
    state = [...state, op];

    try {
      final client = await _ref.read(wingetClientProvider.future);
      final tx = client.installPackage(packageId, silent: true);
      await for (final p in tx.progress) {
        _update(op.id, percent: p.percent, label: p.label);
      }
      await tx.result;
      _update(op.id, status: OperationStatus.completed, percent: 100);
      _ref.invalidate(installedPackagesProvider);
      _ref.invalidate(updatablePackagesProvider);
    } on WgException catch (e) {
      _update(op.id, status: OperationStatus.failed, error: e.message);
    }
  }

  Future<void> upgrade(String packageId) async {
    final op = WingetOperation(
      id: UniqueKey().toString(),
      packageId: packageId,
      type: OperationType.upgrade,
      status: OperationStatus.running,
    );
    state = [...state, op];

    try {
      final client = await _ref.read(wingetClientProvider.future);
      final tx = client.upgradePackage(packageId, silent: true);
      await for (final p in tx.progress) {
        _update(op.id, percent: p.percent, label: p.label);
      }
      await tx.result;
      _update(op.id, status: OperationStatus.completed, percent: 100);
      _ref.invalidate(installedPackagesProvider);
      _ref.invalidate(updatablePackagesProvider);
    } on WgException catch (e) {
      _update(op.id, status: OperationStatus.failed, error: e.message);
    }
  }

  Future<void> uninstall(String packageId) async {
    final op = WingetOperation(
      id: UniqueKey().toString(),
      packageId: packageId,
      type: OperationType.uninstall,
      status: OperationStatus.running,
    );
    state = [...state, op];

    try {
      final client = await _ref.read(wingetClientProvider.future);
      final tx = client.uninstallPackage(packageId, silent: true);
      await for (final p in tx.progress) {
        _update(op.id, percent: p.percent, label: p.label);
      }
      await tx.result;
      _update(op.id, status: OperationStatus.completed, percent: 100);
      _ref.invalidate(installedPackagesProvider);
      _ref.invalidate(updatablePackagesProvider);
    } on WgException catch (e) {
      _update(op.id, status: OperationStatus.failed, error: e.message);
    }
  }

  void clearFinished() {
    state =
        state
            .where(
              (op) =>
                  op.status == OperationStatus.running ||
                  op.status == OperationStatus.pending,
            )
            .toList();
  }

  void _update(
    String id, {
    OperationStatus? status,
    int? percent,
    String? label,
    String? error,
  }) {
    state = [
      for (final op in state)
        if (op.id == id)
          op.copyWith(
            status: status,
            percent: percent,
            label: label,
            error: error,
          )
        else
          op,
    ];
  }
}
