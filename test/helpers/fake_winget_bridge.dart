import 'dart:async';
import 'dart:isolate';
import 'package:winget_dart/src/bridge/winget_bridge.dart';
import 'package:winget_dart/src/exceptions/wg_exception.dart';

/// Canonical JSON message builders for tests.
abstract final class Msg {
  static const done = '{"done":true}';
  static const ok = '{"ok":true}';
  static const success = '{"result":{"success":true}}';
  static const cancelled = '{"cancelled":true}';

  static String error(String msg, {int hresult = 0}) =>
      '{"error":"$msg","hresult":$hresult}';

  static String pkg({
    required String id,
    required String name,
    String version = '1.0.0',
    String? availableVersion,
    String source = 'winget',
    String catalog = 'winget',
  }) {
    final av = availableVersion != null
        ? ',"available_version":"$availableVersion"'
        : '';
    return '{"pkg":{"id":"$id","name":"$name","version":"$version"'
        '$av,"source":"$source","catalog":"$catalog"}}';
  }

  static String catalog(String id, {String? name}) =>
      '{"catalog":{"id":"$id","name":"${name ?? id}"}}';

  static String progress(int percent,
          {String state = 'downloading', String? label}) =>
      '{"progress":{"percent":$percent,"state":"$state",'
      '"label":"${label ?? '$state ($percent%)'}"}}';

  static String plan({
    List<Map<String, String>> installing = const [],
    List<Map<String, String>> upgrading = const [],
    List<Map<String, String>> removing = const [],
  }) {
    String pkgs(List<Map<String, String>> list) =>
        '[${list.map((p) => '{"id":"${p['id']}","name":"${p['name'] ?? p['id']}",'
            '"version":"${p['version'] ?? '1.0.0'}","source":"winget","catalog":"winget"}').join(',')}]';
    return '{"plan":{"installing":${pkgs(installing)},'
        '"upgrading":${pkgs(upgrading)},"removing":${pkgs(removing)}}}';
  }
}

/// Scriptable [WingetBridge] for unit testing.
final class FakeWingetBridge implements WingetBridge {
  bool _available = true;
  int _initResult = 0;
  int _connectHandle = 1;
  bool _connectOk = true;
  String _connectError = '';
  int _connectHresult = 0;

  final _responses = <String, List<String>>{};

  // Stub configuration
  FakeWingetBridge stubIsAvailable(bool v) {
    _available = v;
    return this;
  }

  FakeWingetBridge stubInitResult(int hresult) {
    _initResult = hresult;
    return this;
  }

  FakeWingetBridge stubConnect({
    int handle = 1,
    bool ok = true,
    String errorMessage = 'Connect failed',
    int hresult = -1,
  }) {
    _connectHandle = handle;
    _connectOk = ok;
    _connectError = errorMessage;
    _connectHresult = hresult;
    return this;
  }

  FakeWingetBridge stubCatalogs(List<String> messages) {
    _responses['listCatalogs'] = messages;
    return this;
  }

  FakeWingetBridge stubSearchName(String query, List<String> messages) {
    _responses['searchName:$query'] = messages;
    return this;
  }

  FakeWingetBridge stubFindById(String packageId, String message) {
    _responses['findById:$packageId'] = [message];
    return this;
  }

  FakeWingetBridge stubListInstalled(List<String> messages) {
    _responses['listInstalled'] = messages;
    return this;
  }

  FakeWingetBridge stubGetUpdates(List<String> messages) {
    _responses['getUpdates'] = messages;
    return this;
  }

  FakeWingetBridge stubSimulateInstall(String packageId, String message) {
    _responses['simulateInstall:$packageId'] = [message];
    return this;
  }

  FakeWingetBridge stubInstall(String packageId, List<String> messages) {
    _responses['install:$packageId'] = messages;
    return this;
  }

  FakeWingetBridge stubUpgrade(String packageId, List<String> messages) {
    _responses['upgrade:$packageId'] = messages;
    return this;
  }

  FakeWingetBridge stubUninstall(String packageId, List<String> messages) {
    _responses['uninstall:$packageId'] = messages;
    return this;
  }

  // WingetBridge implementation

  @override
  bool isAvailable() => _available;

  @override
  void init() {
    if (_initResult != 0) {
      throw WgException('Init failed', hresult: _initResult);
    }
  }

  @override
  int connect(SendPort reply) {
    _post(reply, _connectOk
        ? Msg.ok
        : Msg.error(_connectError, hresult: _connectHresult));
    return _connectHandle;
  }

  @override
  void disconnect(int handle) {}

  @override
  void cancel(int handle) {}

  @override
  void listCatalogs(int handle, SendPort reply) =>
      _postAll(reply, _responses['listCatalogs'] ?? [Msg.done]);

  @override
  void searchName(int handle, String query, SendPort reply) =>
      _postAll(reply, _responses['searchName:$query'] ?? [Msg.done]);

  @override
  void findById(int handle, String packageId, String? catalogId,
          SendPort reply) =>
      _postAll(reply,
          _responses['findById:$packageId'] ?? [Msg.error('Not found')]);

  @override
  void listInstalled(int handle, SendPort reply) =>
      _postAll(reply, _responses['listInstalled'] ?? [Msg.done]);

  @override
  void getUpdates(int handle, SendPort reply) =>
      _postAll(reply, _responses['getUpdates'] ?? [Msg.done]);

  @override
  void simulateInstall(int handle, String packageId, String? catalogId,
          String? version, SendPort reply) =>
      _postAll(reply,
          _responses['simulateInstall:$packageId'] ?? [Msg.plan()]);

  @override
  void simulateUpgrade(int handle, String? packageId, SendPort reply) =>
      _postAll(reply, _responses['simulateUpgrade:${packageId ?? '*'}'] ??
          [Msg.plan()]);

  @override
  void install(int handle, String packageId, String? catalogId,
          String? version, bool silent, SendPort reply) =>
      _postAll(
          reply, _responses['install:$packageId'] ?? [Msg.success]);

  @override
  void upgrade(int handle, String packageId, String? version, bool silent,
          SendPort reply) =>
      _postAll(
          reply, _responses['upgrade:$packageId'] ?? [Msg.success]);

  @override
  void uninstall(int handle, String packageId, bool silent, SendPort reply) =>
      _postAll(
          reply, _responses['uninstall:$packageId'] ?? [Msg.success]);

  // Delivery helpers

  static void _post(SendPort port, String json) {
    scheduleMicrotask(() => port.send(json));
  }

  static void _postAll(SendPort port, List<String> messages) {
    var i = 0;
    for (final msg in messages) {
      final index = i++;
      Future<void>.delayed(Duration(microseconds: index * 100))
          .then((_) => port.send(msg));
    }
  }
}
