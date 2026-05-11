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

  static const FirebaseOptions android = FirebaseOptions(
    apiKey:            'AIzaSyBNrDpjZc4OAr4084OoWzdkFtMymseTwcA',
    appId:             '1:439449257622:android:b70de0720bca2b703264b7',
    messagingSenderId: '439449257622',
    projectId:         'assettracker-daf80',
    storageBucket:     'assettracker-daf80.firebasestorage.app',
  );
}
