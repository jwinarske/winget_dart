import 'dart:async';
import 'dart:isolate';
import 'bridge/winget_bridge.dart';
import 'codec/message_decoder.dart';
import 'models/wg_package.dart';
import 'models/wg_install_plan.dart';
import 'models/wg_progress.dart';
import 'models/wg_catalog.dart';
import 'exceptions/wg_exception.dart';
import 'exceptions/wg_not_available.dart';
import 'exceptions/wg_cancelled.dart';
import 'wg_transaction.dart';

/// Primary entry point for winget_dart.
///
/// ```dart
/// import 'package:winget_dart/winget_dart.dart';
/// import 'package:winget_dart/src/bridge/native_winget_bridge.dart';
///
/// final client = await WgClient.connect(NativeWingetBridge());
/// final packages = await client.searchName('cmake').result;
/// for (final p in packages) print('${p.id} ${p.version}');
/// await client.close();
/// ```
class WgClient {
  final int _handle;
  final WingetBridge _bridge;
  bool _closed = false;

  WgClient._(this._handle, this._bridge);

  // ---------------------------------------------------------------------------
  // Lifecycle
  // ---------------------------------------------------------------------------

  /// Connect to the Windows Package Manager.
  ///
  /// Pass a [WingetBridge] implementation — typically [NativeWingetBridge]
  /// for production or a fake for testing.
  ///
  /// Retries with exponential backoff if WinGet is not yet registered
  /// (common on freshly provisioned machines or first login). Set
  /// [maxRetries] to 0 to fail immediately if WinGet is unavailable.
  ///
  /// Throws [WgNotAvailableException] if WinGet (App Installer) is not
  /// present after all retries are exhausted.
  static Future<WgClient> connect(
    WingetBridge bridge, {
    int maxRetries = 0,
    Duration retryDelay = Duration.zero,
  }) async {
    // Initialize the Dart API bridge first so that Dart_PostCObject_DL
    // is available before any WinGet DLLs are loaded.
    bridge.init();

    // Retry loop for the fresh-login race condition (App Installer not
    // yet registered after first login on a fresh Windows installation).
    for (var attempt = 0; attempt <= maxRetries; attempt++) {
      if (bridge.isAvailable()) break;

      if (attempt == maxRetries) {
        throw const WgNotAvailableException(
            'Windows Package Manager (App Installer) is not installed or '
            'not reachable. Windows 10 1809+ (x64) or Windows 11 (ARM64) '
            'required. If this is a freshly provisioned machine, log out '
            'and back in to complete App Installer registration.');
      }
      // Exponential backoff capped at 30 seconds.
      final wait = retryDelay * (attempt + 1);
      await Future<void>.delayed(wait > const Duration(seconds: 30)
          ? const Duration(seconds: 30)
          : wait);
    }

    final port = ReceivePort();
    try {
      final handle = bridge.connect(port.sendPort);
      if (handle < 0) {
        throw WgException(
            'wg_connect returned HRESULT 0x${handle.toRadixString(16)}');
      }

      final msg = await port.first;
      final decoded = MessageDecoder.decode(msg);
      if (decoded['error'] != null) {
        throw WgException(decoded['error'] as String,
            hresult: decoded['hresult'] as int?);
      }

      return WgClient._(handle, bridge);
    } finally {
      port.close();
    }
  }

  /// Disconnect and release COM references.
  Future<void> close() async {
    if (_closed) return;
    _closed = true;
    _bridge.disconnect(_handle);
  }

  // ---------------------------------------------------------------------------
  // Catalogs
  // ---------------------------------------------------------------------------

  /// Returns all configured package catalogs.
  Future<List<WgCatalog>> listCatalogs() async {
    final port = ReceivePort();
    try {
      _bridge.listCatalogs(_handle, port.sendPort);
      final catalogs = <WgCatalog>[];
      await for (final msg in port) {
        final decoded = MessageDecoder.decode(msg);
        if (decoded.containsKey('done')) break;
        if (decoded['error'] != null) {
          throw WgException(decoded['error'] as String);
        }
        catalogs.add(
            WgCatalog.fromJson(decoded['catalog'] as Map<String, dynamic>));
      }
      return catalogs;
    } finally {
      port.close();
    }
  }

  // ---------------------------------------------------------------------------
  // Search
  // ---------------------------------------------------------------------------

  /// Search by name across all catalogs.
  WgTransaction<List<WgPackage>> searchName(String query) {
    return _streamingTransaction(
      (port) => _bridge.searchName(_handle, query, port),
    );
  }

  /// Find a specific package by ID.
  Future<WgPackage?> findById(String packageId, {String? catalogId}) async {
    final port = ReceivePort();
    try {
      _bridge.findById(_handle, packageId, catalogId, port.sendPort);
      final msg = await port.first;
      final decoded = MessageDecoder.decode(msg);
      if (decoded['error'] != null) {
        throw WgException(decoded['error'] as String);
      }
      final pkg = decoded['pkg'];
      return pkg == null
          ? null
          : WgPackage.fromJson(pkg as Map<String, dynamic>);
    } finally {
      port.close();
    }
  }

  // ---------------------------------------------------------------------------
  // Installed packages
  // ---------------------------------------------------------------------------

  /// Returns all locally installed packages.
  WgTransaction<List<WgPackage>> listInstalled() {
    return _streamingTransaction(
      (port) => _bridge.listInstalled(_handle, port),
    );
  }

  // ---------------------------------------------------------------------------
  // Simulate (dry-run)
  // ---------------------------------------------------------------------------

  /// Dry-runs an install to preview what packages would be added or changed.
  Future<WgInstallPlan> simulateInstall(String packageId,
      {String? catalogId, String? version}) async {
    final port = ReceivePort();
    try {
      _bridge.simulateInstall(
          _handle, packageId, catalogId, version, port.sendPort);
      final msg = await port.first;
      final decoded = MessageDecoder.decode(msg);
      if (decoded['error'] != null) {
        throw WgException(decoded['error'] as String);
      }
      return WgInstallPlan.fromJson(decoded['plan'] as Map<String, dynamic>);
    } finally {
      port.close();
    }
  }

  // ---------------------------------------------------------------------------
  // Install / Upgrade / Uninstall
  // ---------------------------------------------------------------------------

  /// Installs a package, emitting progress events.
  WgTransaction<void> installPackage(String packageId,
      {String? catalogId, String? version, bool silent = true}) {
    return _progressTransaction(
      (port) =>
          _bridge.install(_handle, packageId, catalogId, version, silent, port),
    );
  }

  /// Upgrades a package to the latest (or specified) version.
  WgTransaction<void> upgradePackage(String packageId,
      {String? version, bool silent = true}) {
    return _progressTransaction(
      (port) => _bridge.upgrade(_handle, packageId, version, silent, port),
    );
  }

  /// Uninstalls a package.
  WgTransaction<void> uninstallPackage(String packageId, {bool silent = true}) {
    return _progressTransaction(
      (port) => _bridge.uninstall(_handle, packageId, silent, port),
    );
  }

  // ---------------------------------------------------------------------------
  // Updates
  // ---------------------------------------------------------------------------

  /// Returns packages that have available upgrades.
  WgTransaction<List<WgPackage>> getUpdates() {
    return _streamingTransaction(
      (port) => _bridge.getUpdates(_handle, port),
    );
  }

  // ---------------------------------------------------------------------------
  // Cancellation
  // ---------------------------------------------------------------------------

  /// Cancels any in-flight operation.
  void cancel() => _bridge.cancel(_handle);

  // ---------------------------------------------------------------------------
  // Internal helpers
  // ---------------------------------------------------------------------------

  WgTransaction<List<WgPackage>> _streamingTransaction(
      void Function(SendPort port) startFn) {
    final port = ReceivePort();
    final packages = <WgPackage>[];
    final controller = StreamController<WgPackage>();
    final completer = Completer<List<WgPackage>>();

    void cleanup(Object error, [StackTrace? stackTrace]) {
      if (!completer.isCompleted) {
        controller.addError(error, stackTrace);
        controller.close();
        completer.completeError(error, stackTrace);
      }
      port.close();
    }

    startFn(port.sendPort);

    port.listen(
      (msg) {
        try {
          final decoded = MessageDecoder.decode(msg);
          if (decoded.containsKey('done')) {
            controller.close();
            completer.complete(packages);
            port.close();
          } else if (decoded.containsKey('cancelled')) {
            cleanup(const WgCancelledException());
          } else if (decoded['error'] != null) {
            cleanup(WgException(decoded['error'] as String,
                hresult: decoded['hresult'] as int?));
          } else if (decoded['pkg'] != null) {
            final pkg =
                WgPackage.fromJson(decoded['pkg'] as Map<String, dynamic>);
            packages.add(pkg);
            controller.add(pkg);
          }
        } catch (e, st) {
          cleanup(e, st);
        }
      },
      onError: (Object error, StackTrace stackTrace) =>
          cleanup(error, stackTrace),
    );

    return WgTransaction<List<WgPackage>>(
      packages: controller.stream,
      result: completer.future,
      progress: const Stream.empty(),
    );
  }

  WgTransaction<void> _progressTransaction(
      void Function(SendPort port) startFn) {
    final port = ReceivePort();
    final progressController = StreamController<WgProgress>();
    final completer = Completer<void>();

    void cleanup(Object error, [StackTrace? stackTrace]) {
      if (!completer.isCompleted) {
        progressController.addError(error, stackTrace);
        progressController.close();
        completer.completeError(error, stackTrace);
      }
      port.close();
    }

    startFn(port.sendPort);

    port.listen(
      (msg) {
        try {
          final decoded = MessageDecoder.decode(msg);
          if (decoded.containsKey('result')) {
            progressController.close();
            completer.complete();
            port.close();
          } else if (decoded.containsKey('cancelled')) {
            cleanup(const WgCancelledException());
          } else if (decoded['error'] != null) {
            cleanup(WgException(decoded['error'] as String,
                hresult: decoded['hresult'] as int?));
          } else if (decoded['progress'] != null) {
            progressController.add(WgProgress.fromJson(
                decoded['progress'] as Map<String, dynamic>));
          }
        } catch (e, st) {
          cleanup(e, st);
        }
      },
      onError: (Object error, StackTrace stackTrace) =>
          cleanup(error, stackTrace),
    );

    return WgTransaction<void>(
      packages: const Stream.empty(),
      progress: progressController.stream,
      result: completer.future,
    );
  }
}
