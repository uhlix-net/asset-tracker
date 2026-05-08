import 'dart:typed_data';
import 'package:flutter/material.dart';
import '../models/asset.dart';
import '../services/firebase_service.dart';

class AssetDetailScreen extends StatefulWidget {
  final Asset           asset;
  final FirebaseService firebase;
  final Uint8List       syncKey;

  const AssetDetailScreen(
      {super.key,
      required this.asset,
      required this.firebase,
      required this.syncKey});

  @override
  State<AssetDetailScreen> createState() => _AssetDetailScreenState();
}

class _AssetDetailScreenState extends State<AssetDetailScreen> {
  final Map<String, Uint8List?> _cache = {};
  bool _loadingFiles = false;

  static const _navy  = Color(0xFF1B2A3B);
  static const _slate = Color(0xFF4A5568);
  static const _bg    = Color(0xFFF4F6F8);

  @override
  void initState() {
    super.initState();
    _loadFiles();
  }

  Future<void> _loadFiles() async {
    setState(() => _loadingFiles = true);
    for (final f in widget.asset.files) {
      if (f.isImage || f.fileType == 'receipt') {
        final data = await widget.firebase
            .fetchFile(f.assetId, f.storedName, widget.syncKey);
        if (mounted) setState(() => _cache[f.storedName] = data);
      }
    }
    if (mounted) setState(() => _loadingFiles = false);
  }

  @override
  Widget build(BuildContext context) {
    final a = widget.asset;
    return Scaffold(
      backgroundColor: _bg,
      appBar: AppBar(
        backgroundColor: _navy,
        foregroundColor: Colors.white,
        title: Text(a.name, overflow: TextOverflow.ellipsis),
      ),
      body: SingleChildScrollView(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            // Asset ID badge
            Container(
              padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 4),
              decoration: BoxDecoration(
                  color: _navy.withOpacity(0.1),
                  borderRadius: BorderRadius.circular(6)),
              child: Text('Asset ID: ${a.id}',
                  style: const TextStyle(
                      fontWeight: FontWeight.w600, color: _navy, fontSize: 13)),
            ),
            const SizedBox(height: 16),

            // Details card
            _card([
              _row('Name',          a.name),
              _row('Category',      a.category.isNotEmpty ? a.category : '—'),
              _row('Serial Number', a.serialNumber.isNotEmpty ? a.serialNumber : '—'),
              _row('Model Number',  a.modelNumber.isNotEmpty ? a.modelNumber : '—'),
              _row('Purchase Date', a.datePurchase ?? '—'),
              _row('Purchase Price', a.valueDisplay),
              _row('Current Value', a.currentValueDisplay),
              _row('Date Added',    a.dateAdded.substring(0, 10)),
            ]),

            if (a.notes.isNotEmpty) ...[
              const SizedBox(height: 12),
              _sectionTitle('Notes'),
              _card([Padding(
                  padding: const EdgeInsets.all(8),
                  child: Text(a.notes,
                      style: const TextStyle(fontSize: 13, color: _slate)))]),
            ],

            // Photos
            if (widget.asset.files.any((f) => f.fileType == 'image')) ...[
              const SizedBox(height: 16),
              _sectionTitle('Photos'),
              _photoGrid(widget.asset.files
                  .where((f) => f.fileType == 'image')
                  .toList()),
            ],

            // Receipt
            if (widget.asset.files.any((f) => f.fileType == 'receipt')) ...[
              const SizedBox(height: 16),
              _sectionTitle('Sales Receipt'),
              ...widget.asset.files
                  .where((f) => f.fileType == 'receipt')
                  .map((f) => _receiptWidget(f)),
            ],

            const SizedBox(height: 24),
          ],
        ),
      ),
    );
  }

  Widget _sectionTitle(String t) => Padding(
        padding: const EdgeInsets.only(bottom: 8),
        child: Text(t.toUpperCase(),
            style: const TextStyle(
                fontSize: 11,
                fontWeight: FontWeight.bold,
                color: _slate,
                letterSpacing: 1.1)),
      );

  Widget _card(List<Widget> children) => Card(
        margin: EdgeInsets.zero,
        child: Column(children: children),
      );

  Widget _row(String label, String value) => Padding(
        padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 8),
        child: Row(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            SizedBox(
              width: 120,
              child: Text(label,
                  style: const TextStyle(
                      fontSize: 12,
                      fontWeight: FontWeight.w600,
                      color: _slate)),
            ),
            Expanded(
              child: Text(value,
                  style: const TextStyle(fontSize: 13)),
            ),
          ],
        ),
      );

  Widget _photoGrid(List<AssetFile> images) {
    return GridView.builder(
      shrinkWrap: true,
      physics: const NeverScrollableScrollPhysics(),
      gridDelegate: const SliverGridDelegateWithFixedCrossAxisCount(
          crossAxisCount: 3, mainAxisSpacing: 4, crossAxisSpacing: 4),
      itemCount: images.length,
      itemBuilder: (ctx, i) {
        final f     = images[i];
        final bytes = _cache[f.storedName];
        return GestureDetector(
          onTap: bytes == null ? null : () => _fullscreen(bytes, f.fileName),
          child: ClipRRect(
            borderRadius: BorderRadius.circular(6),
            child: bytes != null
                ? Image.memory(bytes, fit: BoxFit.cover)
                : Container(
                    color: const Color(0xFFE0E7EF),
                    child: const Center(
                        child: CircularProgressIndicator(strokeWidth: 2))),
          ),
        );
      },
    );
  }

  Widget _receiptWidget(AssetFile f) {
    final bytes = _cache[f.storedName];
    if (bytes == null) {
      return const Padding(
          padding: EdgeInsets.symmetric(vertical: 8),
          child: Center(child: CircularProgressIndicator(strokeWidth: 2)));
    }
    if (f.isImage) {
      return GestureDetector(
        onTap: () => _fullscreen(bytes, f.fileName),
        child: ClipRRect(
          borderRadius: BorderRadius.circular(8),
          child: Image.memory(bytes, fit: BoxFit.contain),
        ),
      );
    }
    // PDF or other — show a placeholder tile
    return Card(
      child: ListTile(
        leading: const Icon(Icons.picture_as_pdf, color: Colors.red),
        title: Text(f.fileName),
        subtitle: const Text('PDF receipt — open on desktop to view'),
      ),
    );
  }

  void _fullscreen(Uint8List bytes, String name) {
    Navigator.push(
      context,
      MaterialPageRoute(
        builder: (_) => Scaffold(
          backgroundColor: Colors.black,
          appBar: AppBar(
              backgroundColor: Colors.black,
              foregroundColor: Colors.white,
              title: Text(name)),
          body: Center(
              child: InteractiveViewer(
                  child: Image.memory(bytes))),
        ),
      ),
    );
  }
}
