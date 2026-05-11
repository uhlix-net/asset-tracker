import 'dart:typed_data';
import 'package:cloud_firestore/cloud_firestore.dart';
import 'package:firebase_auth/firebase_auth.dart';
import 'package:firebase_storage/firebase_storage.dart';
import '../models/asset.dart';
import 'crypto_service.dart';

class FirebaseService {
  final FirebaseAuth     _auth     = FirebaseAuth.instance;
  final FirebaseFirestore _db      = FirebaseFirestore.instance;
  final FirebaseStorage  _storage  = FirebaseStorage.instance;

  String? get uid => _auth.currentUser?.uid;

  // ── Auth ────────────────────────────────────────────────────────────────────

  Future<void> signIn(String email, String password) =>
      _auth.signInWithEmailAndPassword(email: email, password: password);

  Future<void> signOut() => _auth.signOut();

  bool get isSignedIn => _auth.currentUser != null;

  // ── Asset list ──────────────────────────────────────────────────────────────

  Future<List<Asset>> fetchAssets(Uint8List syncKey) async {
    final snap = await _db
        .collection('users')
        .doc(uid)
        .collection('assets')
        .get();

    if (snap.docs.isEmpty) {
      throw Exception(
          'No synced assets found in Firebase.\n'
          'Please run File → Sync to Cloud from the Windows app first.');
    }

    final assets = <Asset>[];
    int decryptFailures = 0;
    for (final doc in snap.docs) {
      final enc = doc.data()['data'] as String?;
      if (enc == null) continue;
      try {
        final map = CryptoService.decryptJson(enc, syncKey);
        assets.add(Asset.fromMap(map));
      } catch (_) {
        decryptFailures++;
      }
    }

    if (decryptFailures > 0 && assets.isEmpty) {
      throw Exception(
          'Could not decrypt any assets ($decryptFailures of ${snap.docs.length} failed).\n'
          'Check that your Sync Password matches the one used on the Windows app.');
    }

    // Attach file manifests
    final manifestDoc = await _db
        .collection('users')
        .doc(uid)
        .collection('metadata')
        .doc('manifest')
        .get();

    if (manifestDoc.exists) {
      final encManifest = manifestDoc.data()?['data'] as String?;
      if (encManifest != null) {
        try {
          final manifest = CryptoService.decryptJson(encManifest, syncKey)
              as Map<String, dynamic>;
          for (final asset in assets) {
            final fileList = (manifest[asset.id] as List<dynamic>?) ?? [];
            asset.files = fileList
                .map((f) => AssetFile.fromMap(
                    asset.id, Map<String, dynamic>.from(f as Map)))
                .toList();
          }
        } catch (_) {}
      }
    }

    assets.sort((a, b) => b.dateAdded.compareTo(a.dateAdded));
    return assets;
  }

  // ── File download ───────────────────────────────────────────────────────────

  Future<Uint8List?> fetchFile(
      String assetId, String storedName, Uint8List syncKey) async {
    try {
      final ref  = _storage.ref('users/$uid/files/$assetId/$storedName');
      final data = await ref.getData(20 * 1024 * 1024); // 20 MB limit
      if (data == null) return null;
      return CryptoService.decrypt(data, syncKey);
    } catch (_) {
      return null;
    }
  }
}
