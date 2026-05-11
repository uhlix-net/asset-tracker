import 'dart:io';
import 'package:xml/xml.dart';

void main(List<String> args) {
  final manifestPath = args.isNotEmpty ? args[0] : 'android/app/src/main/AndroidManifest.xml';
  final content = File(manifestPath).readAsStringSync();

  print('=== Manifest length: ${content.length} chars ===');
  print('=== XmlDocument.parse ===');

  final doc = XmlDocument.parse(content);

  // Test 1: findAllElements('meta-data')
  final metas = doc.findAllElements('meta-data').toList();
  print('findAllElements("meta-data") count: ${metas.length}');

  for (var i = 0; i < metas.length; i++) {
    final m = metas[i];
    print('\nmeta-data[$i]:');
    print('  getAttribute("android:name") = ${m.getAttribute("android:name")}');
    print('  getAttribute("name", namespace:"*") = ${m.getAttribute("name", namespace: "*")}');
    print('  getAttribute("name", namespace:"http://schemas.android.com/apk/res/android") = '
        '${m.getAttribute("name", namespace: "http://schemas.android.com/apk/res/android")}');
    print('  raw attributes:');
    for (final a in m.attributes) {
      print('    qualified="${a.name.qualified}" local="${a.name.local}" '
            'prefix="${a.name.prefix}" namespaceUri="${a.name.namespaceUri}" value="${a.value}"');
    }
  }

  // Test 2: findAllElements with namespace wildcard
  print('\n=== findAllElements("meta-data", namespace:"*") ===');
  final metasNs = doc.findAllElements('meta-data', namespace: '*').toList();
  print('count: ${metasNs.length}');

  // Test 3: Manual walk of all elements
  print('\n=== All XmlElement nodes in document ===');
  var count = 0;
  for (final node in doc.descendants) {
    if (node is XmlElement) {
      count++;
      print('  element: qualified="${node.name.qualified}" local="${node.name.local}"');
    }
  }
  print('Total elements: $count');

  // Test 4: Check if flutterEmbedding string is in content
  print('\n=== String search in raw manifest ===');
  print('Contains "flutterEmbedding": ${content.contains("flutterEmbedding")}');
  print('Contains "android:value=\\"2\\"": ${content.contains('android:value="2"')}');
  print('Contains "meta-data": ${content.contains("meta-data")}');
}
