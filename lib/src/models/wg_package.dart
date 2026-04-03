/// A WinGet package with its identity, version, and catalog origin.
class WgPackage {
  /// Package identifier (e.g. `Kitware.CMake`).
  final String id;

  /// Display name of the package.
  final String name;

  /// Currently installed version string.
  final String version;

  /// Newer version available for upgrade, or `null` if up to date.
  final String? availableVersion;

  /// Source repository name (e.g. `winget`).
  final String source;

  /// Identifier of the catalog this package belongs to.
  final String catalogId;

  /// Creates a [WgPackage].
  const WgPackage({
    required this.id,
    required this.name,
    required this.version,
    this.availableVersion,
    required this.source,
    required this.catalogId,
  });

  /// Deserializes a [WgPackage] from a JSON map.
  factory WgPackage.fromJson(Map<String, dynamic> j) => WgPackage(
        id: j['id'] as String,
        name: j['name'] as String,
        version: j['version'] as String,
        availableVersion: j['available_version'] as String?,
        source: j['source'] as String? ?? '',
        catalogId: j['catalog'] as String? ?? '',
      );

  @override
  String toString() => 'WgPackage($id $version)';
}
