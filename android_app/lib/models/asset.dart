class Asset {
  final String id;
  final String name;
  final String category;
  final String? datePurchase;
  final double? valueEstimate;
  final double? currentValue;
  final String serialNumber;
  final String modelNumber;
  final bool hasReceipt;
  final String dateAdded;
  final String notes;
  List<AssetFile> files;

  Asset({
    required this.id,
    required this.name,
    required this.category,
    this.datePurchase,
    this.valueEstimate,
    this.currentValue,
    required this.serialNumber,
    required this.modelNumber,
    required this.hasReceipt,
    required this.dateAdded,
    required this.notes,
    this.files = const [],
  });

  factory Asset.fromMap(Map<String, dynamic> m) => Asset(
        id:           m['id'] ?? '',
        name:         m['name'] ?? '',
        category:     m['category'] ?? '',
        datePurchase: (m['date_purchase'] as String?)?.isNotEmpty == true
            ? m['date_purchase'] as String
            : null,
        valueEstimate: double.tryParse(m['value_estimate'] ?? ''),
        currentValue:  double.tryParse(m['current_value'] ?? ''),
        serialNumber: m['serial_number'] ?? '',
        modelNumber:  m['model_number'] ?? '',
        hasReceipt:   (m['has_receipt'] ?? '0') == '1',
        dateAdded:    m['date_added'] ?? '',
        notes:        m['notes'] ?? '',
      );

  String get valueDisplay => valueEstimate != null
      ? '\$${valueEstimate!.toStringAsFixed(2)}'
      : '—';

  String get currentValueDisplay => currentValue != null
      ? '\$${currentValue!.toStringAsFixed(2)}'
      : '—';
}

class AssetFile {
  final String assetId;
  final String fileName;
  final String storedName;
  final String fileType;

  const AssetFile({
    required this.assetId,
    required this.fileName,
    required this.storedName,
    required this.fileType,
  });

  factory AssetFile.fromMap(String assetId, Map<String, dynamic> m) => AssetFile(
        assetId:    assetId,
        fileName:   m['file_name'] ?? '',
        storedName: m['stored_name'] ?? '',
        fileType:   m['file_type'] ?? '',
      );

  bool get isImage => const {'.jpg', '.jpeg', '.png', '.gif', '.bmp', '.webp', '.tiff'}
      .contains('.${storedName.split('.').last.toLowerCase()}');
}
