#pragma once
#include <QMainWindow>

#include "models.h"

class AssetList;
class Database;
class PreviewPanel;
class QAction;
class QLabel;
class QSplitter;
class Toolbar;
class UpdateChecker;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(Database *db, QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void buildMenu();
    void buildUi();
    void buildStatusBar();
    void refreshAssets();
    void updateStatusBar();

    void onAdd();
    void onEdit(const Asset &asset);
    void onDuplicate(const Asset &asset);
    void onDeleteAssets(const QVector<Asset> &assets);
    void onBackup();
    void onRestore();
    void onSync();
    void onExportFiles();
    void onExportCsv();
    void onPrint();
    void onCheckForUpdates(bool announceNoUpdate);
    void onDataReplaced();
    void offerToOpen(const QString &title, const QString &message, const QString &path);

    Database *m_db;
    QString m_search;
    QString m_category;

    Toolbar *m_toolbar;
    AssetList *m_assetList;
    PreviewPanel *m_preview;
    QSplitter *m_splitter;
    QLabel *m_countLabel;
    QLabel *m_valuesLabel;
    UpdateChecker *m_updateChecker = nullptr;
};
