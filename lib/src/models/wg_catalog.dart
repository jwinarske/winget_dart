class WgCatalog {
  final String id;
  final String name;

  const WgCatalog({required this.id, required this.name});

  factory WgCatalog.fromJson(Map<String, dynamic> j) => WgCatalog(
        id: j['id'] as String,
        name: j['name'] as String,
      );

  @override
  String toString() => 'WgCatalog($id)';
}
