import 'dart:isolate';

/// Abstraction over the native winget_nc bridge.
///
/// [NativeWingetBridge] is the real FFI implementation. Tests use
/// [FakeWingetBridge] which posts preset JSON strings to the SendPort.
abstract interface class WingetBridge {
  bool isAvailable();
  void init();
  int connect(SendPort reply);
  void disconnect(int handle);
  void cancel(int handle);
  void listCatalogs(int handle, SendPort reply);
  void searchName(int handle, String query, SendPort reply);
  void findById(int handle, String packageId, String? catalogId,
      SendPort reply);
  void listInstalled(int handle, SendPort reply);
  void getUpdates(int handle, SendPort reply);
  void simulateInstall(int handle, String packageId, String? catalogId,
      String? version, SendPort reply);
  void simulateUpgrade(int handle, String? packageId, SendPort reply);
  void install(int handle, String packageId, String? catalogId,
      String? version, bool silent, SendPort reply);
  void upgrade(int handle, String packageId, String? version, bool silent,
      SendPort reply);
  void uninstall(int handle, String packageId, bool silent, SendPort reply);
}
