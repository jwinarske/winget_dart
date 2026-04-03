import 'dart:isolate';

/// Abstraction over the native winget_nc bridge.
///
/// [NativeWingetBridge] is the real FFI implementation. Tests use
/// [FakeWingetBridge] which posts preset JSON strings to the SendPort.
abstract interface class WingetBridge {
  /// Returns `true` if the Windows Package Manager is installed and reachable.
  bool isAvailable();

  /// Initializes the Dart API bridge (must be called before [connect]).
  void init();

  /// Opens a connection to WinGet and returns a session handle.
  int connect(SendPort reply);

  /// Closes the session identified by [handle].
  void disconnect(int handle);

  /// Cancels any in-flight operation on [handle].
  void cancel(int handle);

  /// Lists all configured package catalogs.
  void listCatalogs(int handle, SendPort reply);

  /// Searches catalogs for packages matching [query].
  void searchName(int handle, String query, SendPort reply);

  /// Looks up a single package by [packageId].
  void findById(
      int handle, String packageId, String? catalogId, SendPort reply);

  /// Lists all locally installed packages.
  void listInstalled(int handle, SendPort reply);

  /// Lists packages with available upgrades.
  void getUpdates(int handle, SendPort reply);

  /// Dry-runs an install to preview dependency changes.
  void simulateInstall(int handle, String packageId, String? catalogId,
      String? version, SendPort reply);

  /// Dry-runs an upgrade to preview dependency changes.
  void simulateUpgrade(int handle, String? packageId, SendPort reply);

  /// Installs a package, posting progress events to [reply].
  void install(int handle, String packageId, String? catalogId, String? version,
      bool silent, SendPort reply);

  /// Upgrades a package, posting progress events to [reply].
  void upgrade(int handle, String packageId, String? version, bool silent,
      SendPort reply);

  /// Uninstalls a package, posting progress events to [reply].
  void uninstall(int handle, String packageId, bool silent, SendPort reply);
}
