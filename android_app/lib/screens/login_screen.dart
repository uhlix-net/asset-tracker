import 'dart:isolate';
import 'dart:typed_data';
import 'package:flutter/material.dart';
import '../services/crypto_service.dart';
import '../services/firebase_service.dart';
import 'asset_list_screen.dart';

class LoginScreen extends StatefulWidget {
  const LoginScreen({super.key});

  @override
  State<LoginScreen> createState() => _LoginScreenState();
}

class _LoginScreenState extends State<LoginScreen> {
  final _emailCtrl    = TextEditingController();
  final _fbPassCtrl   = TextEditingController();
  final _syncPassCtrl = TextEditingController();
  final _firebase     = FirebaseService();

  bool   _loading = false;
  String? _error;

  static const _navy = Color(0xFF1B2A3B);

  Future<void> _login() async {
    final email    = _emailCtrl.text.trim();
    final fbPass   = _fbPassCtrl.text;
    final syncPass = _syncPassCtrl.text;

    if (email.isEmpty || fbPass.isEmpty || syncPass.isEmpty) {
      setState(() => _error = 'All fields are required.');
      return;
    }

    setState(() { _loading = true; _error = null; });

    try {
      await _firebase.signIn(email, fbPass);
    } catch (e) {
      setState(() {
        _loading = false;
        _error   = 'Sign-in failed: ${e.toString()
            .replaceAll(RegExp(r'\[.*?\]'), '').trim()}';
      });
      return;
    }

    // Derive AES key in a background isolate (600k PBKDF2 iterations)
    Isolate.run(() => CryptoService.deriveKey(syncPass)).then((key) {
      if (!mounted) return;
      Navigator.pushReplacement(
        context,
        MaterialPageRoute(
          builder: (_) => AssetListScreen(syncKey: key, firebase: _firebase),
        ),
      );
    }).catchError((e) {
      if (!mounted) return;
      setState(() { _loading = false; _error = e.toString(); });
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: const Color(0xFFF4F6F8),
      body: SafeArea(
        child: SingleChildScrollView(
          padding: const EdgeInsets.all(24),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              const SizedBox(height: 48),
              Text('Asset Tracker',
                  textAlign: TextAlign.center,
                  style: TextStyle(
                      fontSize: 28,
                      fontWeight: FontWeight.bold,
                      color: _navy)),
              const Text('Mobile Companion',
                  textAlign: TextAlign.center,
                  style: TextStyle(fontSize: 14, color: Colors.grey)),
              const SizedBox(height: 40),

              _field('Firebase Email', _emailCtrl,
                  keyboard: TextInputType.emailAddress),
              const SizedBox(height: 12),
              _field('Firebase Password', _fbPassCtrl, obscure: true),
              const SizedBox(height: 12),
              _field('Sync Password', _syncPassCtrl, obscure: true),

              const SizedBox(height: 8),
              Text(
                '🔒  The sync password decrypts your data locally on this device.',
                style: TextStyle(fontSize: 11, color: Colors.grey[600]),
              ),

              if (_error != null) ...[
                const SizedBox(height: 12),
                Text(_error!,
                    style: const TextStyle(color: Colors.red, fontSize: 13)),
              ],

              const SizedBox(height: 24),
              ElevatedButton(
                onPressed: _loading ? null : _login,
                style: ElevatedButton.styleFrom(
                    backgroundColor: _navy,
                    foregroundColor: Colors.white,
                    padding:
                        const EdgeInsets.symmetric(vertical: 14)),
                child: _loading
                    ? const SizedBox(
                        width: 20,
                        height: 20,
                        child: CircularProgressIndicator(
                            strokeWidth: 2, color: Colors.white))
                    : const Text('Sign In & Unlock'),
              ),
            ],
          ),
        ),
      ),
    );
  }

  Widget _field(String label, TextEditingController ctrl,
      {bool obscure = false,
       TextInputType keyboard = TextInputType.text}) =>
      TextField(
        controller:   ctrl,
        obscureText:  obscure,
        keyboardType: keyboard,
        decoration:   InputDecoration(
          labelText: label,
          filled:    true,
          fillColor: Colors.white,
          border: OutlineInputBorder(
              borderRadius: BorderRadius.circular(8)),
          contentPadding:
              const EdgeInsets.symmetric(horizontal: 14, vertical: 12),
        ),
      );
}
