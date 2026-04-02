// coverage:ignore-file
// This file is pure FFI delegation. No logic lives here.
import 'dart:ffi';
import 'dart:isolate';
import 'package:ffi/ffi.dart';
import '../bindings/winget_bindings.dart' as ffi;
import 'winget_bridge.dart';

/// FFI implementation of [WingetBridge].
///
/// Each method is a single delegation call to the @Native binding.
/// Zero branching logic — covered implicitly by integration tests.
final class NativeWingetBridge implements WingetBridge {
  @override
  bool isAvailable() => ffi.wgIsAvailable() != 0;

  @override
  void init() {
    final result = ffi.wgInit(NativeApi.initializeApiDLData);
    if (result != 0) {
      throw Exception(
          'wg_init failed with HRESULT 0x${result.toRadixString(16)}');
    }
  }

  @override
  int connect(SendPort reply) => ffi.wgConnect(reply.nativePort);

  @override
  void disconnect(int handle) => ffi.wgDisconnect(handle);

  @override
  void cancel(int handle) => ffi.wgCancel(handle);

  @override
  void listCatalogs(int handle, SendPort reply) =>
      ffi.wgListCatalogs(handle, reply.nativePort);

  @override
  void searchName(int handle, String query, SendPort reply) =>
      using((a) => ffi.wgSearchName(
          handle, query.toNativeUtf8(allocator: a).cast(), reply.nativePort));

  @override
  void findById(
          int handle, String packageId, String? catalogId, SendPort reply) =>
      using((a) => ffi.wgFindById(
            handle,
            packageId.toNativeUtf8(allocator: a).cast(),
            catalogId?.toNativeUtf8(allocator: a).cast() ?? nullptr.cast(),
            reply.nativePort,
          ));

  @override
  void listInstalled(int handle, SendPort reply) =>
      ffi.wgListInstalled(handle, reply.nativePort);

  @override
  void getUpdates(int handle, SendPort reply) =>
      ffi.wgGetUpdates(handle, reply.nativePort);

  @override
  void simulateInstall(int handle, String packageId, String? catalogId,
          String? version, SendPort reply) =>
      using((a) => ffi.wgSimulateInstall(
            handle,
            packageId.toNativeUtf8(allocator: a).cast(),
            catalogId?.toNativeUtf8(allocator: a).cast() ?? nullptr.cast(),
            version?.toNativeUtf8(allocator: a).cast() ?? nullptr.cast(),
            reply.nativePort,
          ));

  @override
  void simulateUpgrade(int handle, String? packageId, SendPort reply) =>
      using((a) => ffi.wgSimulateUpgrade(
            handle,
            packageId?.toNativeUtf8(allocator: a).cast() ?? nullptr.cast(),
            reply.nativePort,
          ));

  @override
  void install(int handle, String packageId, String? catalogId, String? version,
          bool silent, SendPort reply) =>
      using((a) => ffi.wgInstall(
            handle,
            packageId.toNativeUtf8(allocator: a).cast(),
            catalogId?.toNativeUtf8(allocator: a).cast() ?? nullptr.cast(),
            version?.toNativeUtf8(allocator: a).cast() ?? nullptr.cast(),
            silent ? 1 : 0,
            1, // always accept agreements
            reply.nativePort,
          ));

  @override
  void upgrade(int handle, String packageId, String? version, bool silent,
          SendPort reply) =>
      using((a) => ffi.wgUpgrade(
            handle,
            packageId.toNativeUtf8(allocator: a).cast(),
            version?.toNativeUtf8(allocator: a).cast() ?? nullptr.cast(),
            silent ? 1 : 0,
            1,
            reply.nativePort,
          ));

  @override
  void uninstall(int handle, String packageId, bool silent, SendPort reply) =>
      using((a) => ffi.wgUninstall(
            handle,
            packageId.toNativeUtf8(allocator: a).cast(),
            silent ? 1 : 0,
            reply.nativePort,
          ));
}
