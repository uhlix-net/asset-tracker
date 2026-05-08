import 'dart:convert';
import 'dart:typed_data';
import 'package:encrypt/encrypt.dart' as enc;
import 'package:pointycastle/export.dart';

/// Mirrors the PBKDF2 + AES-256-GCM scheme used by the Windows desktop app.
/// Salt, iteration count, and key length must stay identical on both sides.
class CryptoService {
  static const _saltStr   = 'AssetTrackerSync_v1_uhlix';
  static const _iterations = 600000;
  static const _keyLen     = 32;

  /// Derive a 256-bit key from the sync password (same params as Windows app).
  static Uint8List deriveKey(String password) {
    final salt         = Uint8List.fromList(utf8.encode(_saltStr));
    final passwordBytes = Uint8List.fromList(utf8.encode(password));
    final params       = Pbkdf2Parameters(salt, _iterations, _keyLen);
    final pbkdf2       = PBKDF2KeyDerivator(HMac(SHA256Digest(), 64));
    pbkdf2.init(params);
    return pbkdf2.process(passwordBytes);
  }

  /// Decrypt AES-256-GCM ciphertext.  Layout: [12-byte nonce][ciphertext+tag].
  static Uint8List decrypt(Uint8List data, Uint8List key) {
    final nonce      = IV(data.sublist(0, 12));
    final ciphertext = enc.Encrypted(data.sublist(12));
    final encrypter  = enc.Encrypter(
        enc.AES(enc.Key(key), mode: enc.AESMode.gcm, padding: null));
    return Uint8List.fromList(encrypter.decryptBytes(ciphertext, iv: nonce));
  }

  /// Decrypt a base64-encoded JSON blob and return the decoded map.
  static Map<String, dynamic> decryptJson(String b64, Uint8List key) {
    final raw       = base64Decode(b64);
    final plaintext = decrypt(Uint8List.fromList(raw), key);
    return jsonDecode(utf8.decode(plaintext)) as Map<String, dynamic>;
  }
}
