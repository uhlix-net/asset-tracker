# Firebase Setup Guide

This document explains how to configure Firebase so the Windows desktop app
can sync to the cloud and the Android companion app can read your assets.

---

## 1. Create a Firebase project

1. Go to https://console.firebase.google.com
2. Click **Add project**
3. Give it a name (e.g. `AssetTracker`)
4. Disable Google Analytics (optional)
5. Click **Create project**

---

## 2. Enable Authentication

1. In the Firebase console → **Authentication** → **Get started**
2. Click **Email/Password** → Enable → **Save**
3. Click **Add user**, enter the email + password you will use in the app

---

## 3. Enable Firestore

1. **Firestore Database** → **Create database**
2. Choose **Start in production mode**
3. Pick a region close to you → **Enable**
4. Go to **Rules** and replace with:

```
rules_version = '2';
service cloud.firestore {
  match /databases/{database}/documents {
    match /users/{userId}/{document=**} {
      allow read, write: if request.auth != null
                         && request.auth.uid == userId;
    }
  }
}
```

5. Click **Publish**

---

## 4. Enable Storage

1. **Storage** → **Get started**
2. Choose **Start in production mode** → same region → **Done**
3. Go to **Rules** and replace with:

```
rules_version = '2';
service firebase.storage {
  match /b/{bucket}/o {
    match /users/{userId}/{allPaths=**} {
      allow read, write: if request.auth != null
                         && request.auth.uid == userId;
    }
  }
}
```

4. Click **Publish**

---

## 5. Get your Web API Key

1. **Project settings** (gear icon) → **General**
2. Under "Your apps" click **Add app** → **Web** `</>`
3. Register the app with a nickname (e.g. `AssetTracker-Desktop`)
4. Copy the `apiKey` value — you will paste it into the Windows app

---

## 6. Configure the Windows Desktop App

Open Asset Tracker → **File → Sync to Cloud (Firebase)…**

Fill in:

| Field | Where to find it |
|---|---|
| Firebase API Key | Step 5 — the `apiKey` value |
| Project ID | Project settings → General → Project ID |
| Storage Bucket | Storage → `your-project-id.appspot.com` |
| Firebase Email | The email you created in Step 2 |
| Firebase Password | The password you created in Step 2 |
| Sync Password | **Choose any strong password** — this encrypts your data end-to-end before it leaves your machine |

Click **Save Settings**, then **Sync Now**.

---

## 7. Configure the Android App

### Option A — Build yourself (requires Flutter)

1. Install Flutter: https://docs.flutter.dev/get-started/install
2. From Firebase Console → **Project settings** → **Add app** → **Android**
   - Package name: `net.uhlix.assettracker`
   - Download `google-services.json`
3. Replace `android_app/android/app/google-services.json` with the downloaded file
4. Run `flutterfire configure` in `android_app/` to update `lib/firebase_options.dart`
5. Run `flutter build apk --release` in `android_app/`
6. Transfer the APK to your phone and install it

### Option B — GitHub Actions (automatic)

1. Fork / push this repo to GitHub
2. Go to **Settings → Secrets → Actions → New repository secret**
3. Name: `GOOGLE_SERVICES_JSON`
4. Value: the *contents* of your `google-services.json` (paste the JSON text, not base64)

   > Tip: `cat android_app/android/app/google-services.json | base64 -w0`
   > then store the base64 output as the secret value and the workflow will decode it.

5. The APK is built automatically on every push to `main` that touches `android_app/`
6. Download it from **Actions → Build Android APK → Artifacts**

---

## 8. First run on Android

1. Install the APK on your phone (enable "Install from unknown sources" if needed)
2. Enter your Firebase Email, Firebase Password, and **the same Sync Password you set in the Windows app**
3. Tap **Sign In & Unlock** — your assets will load, decrypted locally on-device

---

## Security notes

- Your **Sync Password** encrypts all data with AES-256-GCM before it leaves Windows.
  Google only ever stores ciphertext.
- The sync password is stored in Android's hardware-backed Keystore after first entry.
- If you change the sync password you must re-sync from Windows and re-enter on Android.
- If you forget the sync password, the cloud data is unrecoverable
  (your local Windows data is unaffected).
