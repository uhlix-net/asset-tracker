#pragma once
#include <QTemporaryDir>
#include <QWidget>
#include <memory>
#include <optional>

#include "models.h"

class Database;
class QGridLayout;
class QLabel;
class QPushButton;
class QScrollArea;
class QTextEdit;
class QTimer;

class PreviewPanel : public QWidget {
    Q_OBJECT
public:
    explicit PreviewPanel(Database *db, QWidget *parent = nullptr);

    void showAsset(const Asset &asset);
    void clear();

private:
    void buildUi();
    void loadPhotos(const Asset &asset, const QVector<AssetFile> &files);
    void clearPhotos();
    void updateScrollArea();
    void saveNotes();
    void printAsset();
    void openReceipt();
    void openFolder();

    Database *m_db;
    std::optional<Asset> m_asset;
    std::optional<AssetFile> m_receipt;
    // Decrypted receipts for the default viewer. Removed when the app exits.
    std::unique_ptr<QTemporaryDir> m_tempDir;

    QLabel *m_id, *m_name, *m_category, *m_purchase, *m_value, *m_current;
    QLabel *m_serial, *m_model, *m_receiptLabel, *m_added;
    QLabel *m_emptyState;
    QScrollArea *m_scroll;
    QGridLayout *m_photos;
    QTextEdit *m_notes;
    QTimer *m_notesTimer;
    QPushButton *m_open;
    QPushButton *m_print;
};
