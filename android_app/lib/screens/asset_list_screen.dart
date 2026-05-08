import 'dart:typed_data';
import 'package:flutter/material.dart';
import '../models/asset.dart';
import '../services/firebase_service.dart';
import 'asset_detail_screen.dart';
import 'login_screen.dart';

class AssetListScreen extends StatefulWidget {
  final Uint8List syncKey;
  final FirebaseService firebase;

  const AssetListScreen(
      {super.key, required this.syncKey, required this.firebase});

  @override
  State<AssetListScreen> createState() => _AssetListScreenState();
}

class _AssetListScreenState extends State<AssetListScreen> {
  List<Asset> _assets     = [];
  List<Asset> _filtered   = [];
  bool        _loading    = true;
  String?     _error;
  String      _query      = '';
  String      _category   = 'All';

  static const _navy = Color(0xFF1B2A3B);

  @override
  void initState() {
    super.initState();
    _loadAssets();
  }

  Future<void> _loadAssets() async {
    setState(() { _loading = true; _error = null; });
    try {
      final assets = await widget.firebase.fetchAssets(widget.syncKey);
      setState(() {
        _assets   = assets;
        _loading  = false;
      });
      _applyFilter();
    } catch (e) {
      setState(() { _loading = false; _error = e.toString(); });
    }
  }

  void _applyFilter() {
    setState(() {
      _filtered = _assets.where((a) {
        final matchQ = _query.isEmpty ||
            a.name.toLowerCase().contains(_query.toLowerCase()) ||
            a.serialNumber.toLowerCase().contains(_query.toLowerCase());
        final matchC = _category == 'All' || a.category == _category;
        return matchQ && matchC;
      }).toList();
    });
  }

  List<String> get _categories {
    final cats = {'All', ..._assets.map((a) => a.category).where((c) => c.isNotEmpty)};
    return cats.toList()..sort();
  }

  Future<void> _signOut() async {
    await widget.firebase.signOut();
    if (!mounted) return;
    Navigator.pushReplacement(
        context, MaterialPageRoute(builder: (_) => const LoginScreen()));
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: const Color(0xFFF4F6F8),
      appBar: AppBar(
        backgroundColor: _navy,
        foregroundColor: Colors.white,
        title: const Text('Asset Tracker'),
        actions: [
          IconButton(
            icon: const Icon(Icons.refresh),
            tooltip: 'Refresh',
            onPressed: _loadAssets,
          ),
          IconButton(
            icon: const Icon(Icons.logout),
            tooltip: 'Sign out',
            onPressed: _signOut,
          ),
        ],
      ),
      body: Column(
        children: [
          _buildSearchBar(),
          if (_loading)
            const Expanded(child: Center(child: CircularProgressIndicator()))
          else if (_error != null)
            Expanded(child: Center(
                child: Text(_error!, style: const TextStyle(color: Colors.red))))
          else if (_filtered.isEmpty)
            const Expanded(
                child: Center(child: Text('No assets found.',
                    style: TextStyle(color: Colors.grey))))
          else
            Expanded(child: _buildList()),
        ],
      ),
      bottomNavigationBar: Padding(
        padding: const EdgeInsets.all(8),
        child: Text('${_filtered.length} of ${_assets.length} assets',
            textAlign: TextAlign.center,
            style: const TextStyle(color: Colors.grey, fontSize: 12)),
      ),
    );
  }

  Widget _buildSearchBar() => Container(
        color: Colors.white,
        padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
        child: Column(children: [
          TextField(
            decoration: InputDecoration(
              hintText: 'Search by name or serial…',
              prefixIcon: const Icon(Icons.search),
              filled: true,
              fillColor: const Color(0xFFF4F6F8),
              border: OutlineInputBorder(
                  borderRadius: BorderRadius.circular(8),
                  borderSide: BorderSide.none),
              contentPadding:
                  const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
            ),
            onChanged: (v) { _query = v; _applyFilter(); },
          ),
          const SizedBox(height: 6),
          SingleChildScrollView(
            scrollDirection: Axis.horizontal,
            child: Row(
              children: _categories.map((c) => Padding(
                padding: const EdgeInsets.only(right: 6),
                child: FilterChip(
                  label: Text(c),
                  selected: _category == c,
                  selectedColor: _navy.withOpacity(0.15),
                  onSelected: (_) { _category = c; _applyFilter(); },
                ),
              )).toList(),
            ),
          ),
        ]),
      );

  Widget _buildList() => ListView.builder(
        itemCount: _filtered.length,
        itemBuilder: (ctx, i) {
          final a = _filtered[i];
          return Card(
            margin: const EdgeInsets.symmetric(horizontal: 12, vertical: 4),
            child: ListTile(
              leading: _ThumbnailWidget(
                  asset: a, firebase: widget.firebase, syncKey: widget.syncKey),
              title:    Text(a.name,
                  style: const TextStyle(fontWeight: FontWeight.w600)),
              subtitle: Text(
                  '${a.category.isNotEmpty ? a.category : "—"}  •  ${a.valueDisplay}'),
              trailing: const Icon(Icons.chevron_right),
              onTap: () => Navigator.push(
                ctx,
                MaterialPageRoute(
                  builder: (_) => AssetDetailScreen(
                      asset: a,
                      firebase: widget.firebase,
                      syncKey: widget.syncKey),
                ),
              ),
            ),
          );
        },
      );
}

// ── Thumbnail widget ──────────────────────────────────────────────────────────

class _ThumbnailWidget extends StatefulWidget {
  final Asset          asset;
  final FirebaseService firebase;
  final Uint8List      syncKey;

  const _ThumbnailWidget(
      {required this.asset, required this.firebase, required this.syncKey});

  @override
  State<_ThumbnailWidget> createState() => _ThumbnailWidgetState();
}

class _ThumbnailWidgetState extends State<_ThumbnailWidget> {
  Uint8List? _bytes;
  bool       _tried = false;

  @override
  void initState() {
    super.initState();
    _load();
  }

  Future<void> _load() async {
    final img = widget.asset.files
        .where((f) => f.fileType == 'image' && f.isImage)
        .firstOrNull;
    if (img == null) return;
    final data = await widget.firebase.fetchFile(
        img.assetId, img.storedName, widget.syncKey);
    if (mounted) setState(() { _bytes = data; _tried = true; });
  }

  @override
  Widget build(BuildContext context) {
    if (_bytes != null) {
      return ClipRRect(
        borderRadius: BorderRadius.circular(6),
        child: Image.memory(_bytes!, width: 48, height: 48, fit: BoxFit.cover),
      );
    }
    return Container(
      width: 48, height: 48,
      decoration: BoxDecoration(
          color: const Color(0xFFE8EDF2),
          borderRadius: BorderRadius.circular(6)),
      child: const Icon(Icons.photo, color: Colors.grey),
    );
  }
}
