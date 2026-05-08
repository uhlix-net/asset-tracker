// PLACEHOLDER — replace with output of:
//   flutterfire configure
// after setting up your Firebase project.
//
// See FIREBASE_SETUP.md for step-by-step instructions.

import 'package:firebase_core/firebase_core.dart' show FirebaseOptions;
import 'package:flutter/foundation.dart'
    show defaultTargetPlatform, kIsWeb, TargetPlatform;

class DefaultFirebaseOptions {
  static FirebaseOptions get currentPlatform {
    if (kIsWeb) throw UnsupportedError('Web not supported.');
    switch (defaultTargetPlatform) {
      case TargetPlatform.android:
        return android;
      default:
        throw UnsupportedError(
            'Unsupported platform: $defaultTargetPlatform');
    }
  }

  // Replace these placeholder values with your real Firebase project values.
  // Run `flutterfire configure` to generate this file automatically.
  static const FirebaseOptions android = FirebaseOptions(
    apiKey:            'YOUR_ANDROID_API_KEY',
    appId:             '1:000000000000:android:0000000000000000',
    messagingSenderId: '000000000000',
    projectId:         'your-firebase-project-id',
    storageBucket:     'your-firebase-project-id.appspot.com',
  );
}
