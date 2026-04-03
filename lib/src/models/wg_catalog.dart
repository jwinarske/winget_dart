/// A WinGet package catalog (e.g. community repository or Microsoft Store).
class WgCatalog {
  /// Unique identifier for this catalog (e.g. `winget`, `msstore`).
  final String id;

  /// Display name of the catalog.
  final String name;

  /// Creates a [WgCatalog] with the given [id] and [name].
  const WgCatalog({required this.id, required this.name});

  /// Deserializes a [WgCatalog] from a JSON map.
  factory WgCatalog.fromJson(Map<String, dynamic> j) => WgCatalog(
        id: j['id'] as String,
        name: j['name'] as String,
      );

  @override
  String toString() => 'WgCatalog($id)';
}
