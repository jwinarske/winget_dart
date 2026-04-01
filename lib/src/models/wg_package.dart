class WgPackage {
  final String id;
  final String name;
  final String version;
  final String? availableVersion;
  final String source;
  final String catalogId;

  const WgPackage({
    required this.id,
    required this.name,
    required this.version,
    this.availableVersion,
    required this.source,
    required this.catalogId,
  });

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
