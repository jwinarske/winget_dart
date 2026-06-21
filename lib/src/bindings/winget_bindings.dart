// lib/src/bindings/winget_bindings.dart
//
// FFI bindings to winget_nc.dll.
//
// Uses explicit DynamicLibrary.open() via library_loader.dart instead of
// @Native / @DefaultAsset code-assets, so the DLL is discoverable in all
// execution modes: dart run, dart test, dart pub global run, and AOT-compiled
// executables. The code-assets approach only works when the Dart VM has
// build-hook output available, which excludes dart pub global activate.
//
// Regenerate the function signatures from native/src/winget_bridge.h with
// ffigen, then convert the @Native externals to lookupFunction calls.

import 'dart:ffi' as ffi;

import 'library_loader.dart';

final ffi.DynamicLibrary _lib = loadWingetNc();

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

final _wgInit = _lib.lookupFunction<ffi.Int32 Function(ffi.Pointer<ffi.Void>),
    int Function(ffi.Pointer<ffi.Void>)>('wg_init');

int wgInit(ffi.Pointer<ffi.Void> postCObject) => _wgInit(postCObject);

final _wgConnect =
    _lib.lookupFunction<ffi.Int64 Function(ffi.Int64), int Function(int)>(
        'wg_connect');

int wgConnect(int replyPort) => _wgConnect(replyPort);

final _wgDisconnect =
    _lib.lookupFunction<ffi.Void Function(ffi.Int64), void Function(int)>(
        'wg_disconnect');

void wgDisconnect(int handle) => _wgDisconnect(handle);

final _wgIsAvailable = _lib
    .lookupFunction<ffi.Int32 Function(), int Function()>('wg_is_available');

int wgIsAvailable() => _wgIsAvailable();

// ---------------------------------------------------------------------------
// Catalogs
// ---------------------------------------------------------------------------

final _wgListCatalogs = _lib.lookupFunction<
    ffi.Void Function(ffi.Int64, ffi.Int64),
    void Function(int, int)>('wg_list_catalogs');

void wgListCatalogs(int handle, int replyPort) =>
    _wgListCatalogs(handle, replyPort);

// ---------------------------------------------------------------------------
// Search
// ---------------------------------------------------------------------------

final _wgSearchName = _lib.lookupFunction<
    ffi.Void Function(ffi.Int64, ffi.Pointer<ffi.Char>, ffi.Int64),
    void Function(int, ffi.Pointer<ffi.Char>, int)>('wg_search_name');

void wgSearchName(int handle, ffi.Pointer<ffi.Char> query, int replyPort) =>
    _wgSearchName(handle, query, replyPort);

final _wgFindById = _lib.lookupFunction<
    ffi.Void Function(
        ffi.Int64, ffi.Pointer<ffi.Char>, ffi.Pointer<ffi.Char>, ffi.Int64),
    void Function(int, ffi.Pointer<ffi.Char>, ffi.Pointer<ffi.Char>,
        int)>('wg_find_by_id');

void wgFindById(int handle, ffi.Pointer<ffi.Char> packageId,
        ffi.Pointer<ffi.Char> catalogId, int replyPort) =>
    _wgFindById(handle, packageId, catalogId, replyPort);

// ---------------------------------------------------------------------------
// Installed packages
// ---------------------------------------------------------------------------

final _wgListInstalled = _lib.lookupFunction<
    ffi.Void Function(ffi.Int64, ffi.Int64),
    void Function(int, int)>('wg_list_installed');

void wgListInstalled(int handle, int replyPort) =>
    _wgListInstalled(handle, replyPort);

// ---------------------------------------------------------------------------
// Simulate
// ---------------------------------------------------------------------------

final _wgSimulateInstall = _lib.lookupFunction<
    ffi.Void Function(ffi.Int64, ffi.Pointer<ffi.Char>, ffi.Pointer<ffi.Char>,
        ffi.Pointer<ffi.Char>, ffi.Int64),
    void Function(int, ffi.Pointer<ffi.Char>, ffi.Pointer<ffi.Char>,
        ffi.Pointer<ffi.Char>, int)>('wg_simulate_install');

void wgSimulateInstall(
        int handle,
        ffi.Pointer<ffi.Char> packageId,
        ffi.Pointer<ffi.Char> catalogId,
        ffi.Pointer<ffi.Char> version,
        int replyPort) =>
    _wgSimulateInstall(handle, packageId, catalogId, version, replyPort);

final _wgSimulateUpgrade = _lib.lookupFunction<
    ffi.Void Function(ffi.Int64, ffi.Pointer<ffi.Char>, ffi.Int64),
    void Function(int, ffi.Pointer<ffi.Char>, int)>('wg_simulate_upgrade');

void wgSimulateUpgrade(
        int handle, ffi.Pointer<ffi.Char> packageId, int replyPort) =>
    _wgSimulateUpgrade(handle, packageId, replyPort);

// ---------------------------------------------------------------------------
// Install / Upgrade / Uninstall
// ---------------------------------------------------------------------------

final _wgInstall = _lib.lookupFunction<
    ffi.Void Function(ffi.Int64, ffi.Pointer<ffi.Char>, ffi.Pointer<ffi.Char>,
        ffi.Pointer<ffi.Char>, ffi.Int32, ffi.Int32, ffi.Int64),
    void Function(int, ffi.Pointer<ffi.Char>, ffi.Pointer<ffi.Char>,
        ffi.Pointer<ffi.Char>, int, int, int)>('wg_install');

void wgInstall(
        int handle,
        ffi.Pointer<ffi.Char> packageId,
        ffi.Pointer<ffi.Char> catalogId,
        ffi.Pointer<ffi.Char> version,
        int silent,
        int acceptAgreements,
        int replyPort) =>
    _wgInstall(handle, packageId, catalogId, version, silent, acceptAgreements,
        replyPort);

final _wgUpgrade = _lib.lookupFunction<
    ffi.Void Function(ffi.Int64, ffi.Pointer<ffi.Char>, ffi.Pointer<ffi.Char>,
        ffi.Int32, ffi.Int32, ffi.Int64),
    void Function(int, ffi.Pointer<ffi.Char>, ffi.Pointer<ffi.Char>, int, int,
        int)>('wg_upgrade');

void wgUpgrade(
        int handle,
        ffi.Pointer<ffi.Char> packageId,
        ffi.Pointer<ffi.Char> version,
        int silent,
        int acceptAgreements,
        int replyPort) =>
    _wgUpgrade(handle, packageId, version, silent, acceptAgreements, replyPort);

final _wgUninstall = _lib.lookupFunction<
    ffi.Void Function(ffi.Int64, ffi.Pointer<ffi.Char>, ffi.Int32, ffi.Int64),
    void Function(int, ffi.Pointer<ffi.Char>, int, int)>('wg_uninstall');

void wgUninstall(int handle, ffi.Pointer<ffi.Char> packageId, int silent,
        int replyPort) =>
    _wgUninstall(handle, packageId, silent, replyPort);

// ---------------------------------------------------------------------------
// Updates check
// ---------------------------------------------------------------------------

final _wgGetUpdates = _lib.lookupFunction<
    ffi.Void Function(ffi.Int64, ffi.Int64),
    void Function(int, int)>('wg_get_updates');

void wgGetUpdates(int handle, int replyPort) =>
    _wgGetUpdates(handle, replyPort);

// ---------------------------------------------------------------------------
// Cancellation
// ---------------------------------------------------------------------------

final _wgCancel =
    _lib.lookupFunction<ffi.Void Function(ffi.Int64), void Function(int)>(
        'wg_cancel');

void wgCancel(int handle) => _wgCancel(handle);
