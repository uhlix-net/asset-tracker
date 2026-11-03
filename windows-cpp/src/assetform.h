#pragma once
#include <QDialog>
#include <QStringList>
#include <QVector>
#include <optional>

#include "models.h"

class Database;
class QCheckBox;
class QComboBox;
class QDateEdit;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

// Add and Edit share this dialog: existing is empty when adding.
class AssetForm : public QDialog {
    Q_OBJECT
public:
    explicit AssetForm(Database *db, const std::optional<Asset> &existing = std::nullopt,
                       QWidget *parent = nullptr);

private:
    void buildUi();
    void pickPhotos();
    void removeNewPhotos();
    void removeExistingPhotos();
    void pickReceipt();
    void onAccept();
    Asset formAsset() const;
    void saveNew(const Asset &asset);
    void saveEdit(const Asset &updated);

    Database *m_db;
    std::optional<Asset> m_existing;
    QString m_assetId;
    QVector<AssetFile> m_existingFiles;
    QVector<AssetFile> m_filesToRemove;
    QStringList m_newPhotos;
    QString m_newReceipt;

    QLineEdit *m_name;
    QComboBox *m_category;
    QLineEdit *m_serial;
    QLineEdit *m_model;
    QDateEdit *m_date;
    QCheckBox *m_noDate;
    QDoubleSpinBox *m_value;
    QCheckBox *m_noValue;
    QDoubleSpinBox *m_current;
    QCheckBox *m_noCurrent;
    QCheckBox *m_hasReceipt;
    QLabel *m_receiptLabel;
    QListWidget *m_existingList = nullptr;
    QListWidget *m_newPhotoList;
};
