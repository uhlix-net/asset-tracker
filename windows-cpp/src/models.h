#pragma once
#include <QString>
#include <QVector>
#include <optional>

// Asset folders are named "<id>_<slug>", so this must keep producing the names
// existing folders already have.
QString slugify(const QString &text);

// "$1,234.50", or an em dash when there is no value.
QString moneyDisplay(std::optional<double> value);

QString utcNow();    // ISO-8601 UTC, to the second

struct Asset {
    QString id;                          // Zero-padded 5-digit string: "00001"
    QString name;
    QString category;                    // e.g. "Living Room", "Kitchen"
    QString datePurchase;                // "yyyy-MM-dd", empty when unknown
    std::optional<double> valueEstimate; // Purchase / estimated value
    std::optional<double> currentValue;  // Current / appraised value
    QString serialNumber;
    QString modelNumber;
    bool hasReceipt = false;
    QString dateAdded;                   // ISO-8601 UTC
    QString notes;

    QString dirName() const { return id + "_" + slugify(name); }
};

struct AssetFile {
    qint64 id = -1;                      // -1 before DB insert
    QString assetId;
    QString fileName;                    // Original filename
    QString fileType;                    // "image" | "receipt"
    QString storedName;                  // Filename inside the asset's directory
    QString dateAdded;                   // ISO-8601 UTC
    bool encrypted = false;              // True when stored with Windows DPAPI
};

struct AssetRecord {
    Asset asset;
    QVector<AssetFile> files;
};
