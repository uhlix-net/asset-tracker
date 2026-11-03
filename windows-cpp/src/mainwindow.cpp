#include "mainwindow.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPointer>
#include <QSplitter>
#include <QStatusBar>
#include <QUrl>
#include <QVBoxLayout>

#include "aboutdialog.h"
#include "assetform.h"
#include "assetlist.h"
#include "backup.h"
#include "busy.h"
#include "config.h"
#include "csvexport.h"
#include "database.h"
#include "helpdialog.h"
#include "insurerinfodialog.h"
#include "previewpanel.h"
#include "report.h"
#include "storage.h"
#include "syncdialog.h"
#include "toolbar.h"
#include "updatechecker.h"
#include "updatehistorydialog.h"

MainWindow::MainWindow(Database *db, QWidget *parent)
    : QMainWindow(parent)
    , m_db(db)
{
    setWindowTitle(QString("%1 v%2").arg(Config::kAppName, Config::kAppVersion));
    resize(1200, 720);

    const auto settings = Config::openSettings();
    if (settings->contains("geometry"))
        restoreGeometry(settings->value("geometry").toByteArray());

    buildMenu();
    buildUi();
    buildStatusBar();
    refreshAssets();

    if (!Config::isTestRun()) onCheckForUpdates(false);
}

void MainWindow::buildMenu() {
    QMenu *file = menuBar()->addMenu("File");
    file->addAction("Export to CSV...", this, &MainWindow::onExportCsv);
    file->addAction("Export Asset Files...", this, &MainWindow::onExportFiles);
    file->addSeparator();
    file->addAction("Sync to Cloud (Firebase)...", this, &MainWindow::onSync);
    file->addSeparator();
    file->addAction("Create Backup...", this, &MainWindow::onBackup);
    file->addAction("Restore from Backup...", this, &MainWindow::onRestore);
    file->addSeparator();

    QAction *autoBackup = file->addAction("Auto-backup on Exit");
    autoBackup->setCheckable(true);
    autoBackup->setChecked(Config::openSettings()->value("auto_backup_on_exit", false).toBool());
    connect(autoBackup, &QAction::toggled, this, [](bool checked) {
        Config::openSettings()->setValue("auto_backup_on_exit", checked);
    });

    file->addSeparator();
    file->addAction("Exit", this, &QWidget::close);

    QMenu *help = menuBar()->addMenu("Help");
    help->addAction(QString("%1 Help").arg(Config::kAppName), this,
                    [this] { HelpDialog(this).exec(); });
    help->addAction("Check for Updates...", this, [this] { onCheckForUpdates(true); });
    help->addSeparator();
    help->addAction(QString("About %1").arg(Config::kAppName), this,
                    [this] { AboutDialog(this).exec(); });
    help->addAction("Update History", this, [this] { UpdateHistoryDialog(this).exec(); });
}

void MainWindow::buildUi() {
    auto *central = new QWidget;
    setCentralWidget(central);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_toolbar = new Toolbar;
    root->addWidget(m_toolbar);

    m_splitter = new QSplitter(Qt::Horizontal);
    m_splitter->setHandleWidth(1);
    m_splitter->setChildrenCollapsible(false);
    m_assetList = new AssetList(m_db);
    m_preview = new PreviewPanel(m_db);
    m_splitter->addWidget(m_assetList);
    m_splitter->addWidget(m_preview);
    m_splitter->setStretchFactor(0, 3);
    m_splitter->setStretchFactor(1, 2);

    const auto settings = Config::openSettings();
    if (settings->contains("splitter")) {
        m_splitter->restoreState(settings->value("splitter").toByteArray());
    } else {
        m_splitter->setSizes({width() * 3 / 5, width() * 2 / 5});
    }
    root->addWidget(m_splitter, 1);

    connect(m_toolbar, &Toolbar::addClicked, this, &MainWindow::onAdd);
    connect(m_toolbar, &Toolbar::editClicked, this, [this] {
        if (const auto asset = m_assetList->selectedAsset()) onEdit(*asset);
    });
    connect(m_toolbar, &Toolbar::duplicateClicked, this, [this] {
        if (const auto asset = m_assetList->selectedAsset()) onDuplicate(*asset);
    });
    connect(m_toolbar, &Toolbar::deleteClicked, this, [this] {
        onDeleteAssets(m_assetList->selectedAssets());
    });
    connect(m_toolbar, &Toolbar::backupClicked, this, &MainWindow::onBackup);
    connect(m_toolbar, &Toolbar::printClicked, this, &MainWindow::onPrint);
    connect(m_toolbar, &Toolbar::searchChanged, this, [this](const QString &text) {
        m_search = text;
        refreshAssets();
    });
    connect(m_toolbar, &Toolbar::categoryChanged, this, [this](const QString &category) {
        m_category = category;
        refreshAssets();
    });

    connect(m_assetList, &AssetList::assetSelected, this, [this](const Asset &asset) {
        m_toolbar->setAssetActionsEnabled(true);
        m_preview->showAsset(asset);
    });
    connect(m_assetList, &AssetList::selectionCleared, this, [this] {
        m_toolbar->setAssetActionsEnabled(false);
        m_preview->clear();
    });
    connect(m_assetList, &AssetList::editRequested, this, &MainWindow::onEdit);
    connect(m_assetList, &AssetList::duplicateRequested, this, &MainWindow::onDuplicate);
    connect(m_assetList, &AssetList::deleteRequested, this, &MainWindow::onDeleteAssets);
}

void MainWindow::buildStatusBar() {
    m_countLabel = new QLabel;
    m_countLabel->setContentsMargins(8, 2, 0, 2);
    m_valuesLabel = new QLabel;
    m_valuesLabel->setContentsMargins(0, 2, 8, 2);
    statusBar()->addWidget(m_countLabel);
    statusBar()->addPermanentWidget(m_valuesLabel);
}

void MainWindow::refreshAssets() {
    m_assetList->loadAssets(m_db->getAllAssets(m_search, m_category));
    updateStatusBar();
}

void MainWindow::updateStatusBar() {
    const Totals totals = m_db->getTotals();
    m_countLabel->setText(QString("%1 asset%2").arg(totals.count)
                              .arg(totals.count == 1 ? "" : "s"));
    QStringList parts;
    if (totals.purchaseTotal) parts << "Purchase total  " + moneyDisplay(totals.purchaseTotal);
    if (totals.currentTotal) parts << "Current value  " + moneyDisplay(totals.currentTotal);
    m_valuesLabel->setText(parts.join("     ·     "));
}

void MainWindow::onAdd() {
    if (AssetForm(m_db, std::nullopt, this).exec())
        refreshAssets();
}

void MainWindow::onEdit(const Asset &asset) {
    // The list holds a copy, so notes typed since it loaded are only in the database.
    const auto current = m_db->getAssetById(asset.id);
    if (!current || !AssetForm(m_db, current, this).exec()) return;
    refreshAssets();
    if (const auto updated = m_db->getAssetById(asset.id))
        m_preview->showAsset(*updated);
}

void MainWindow::onDuplicate(const Asset &asset) {
    Asset copy = asset;
    copy.id = m_db->nextAssetId();
    copy.name = asset.name + " (Copy)";
    copy.hasReceipt = false;
    copy.dateAdded = utcNow();
    try {
        m_db->insertAsset(copy);
    } catch (const std::exception &e) {
        QMessageBox::critical(this, "Duplicate Failed", e.what());
    }
    refreshAssets();
}

void MainWindow::onDeleteAssets(const QVector<Asset> &assets) {
    if (assets.isEmpty()) return;

    QStringList names;
    for (int i = 0; i < qMin<qsizetype>(assets.size(), 10); ++i)
        names << QString("  • %1 (%2)").arg(assets[i].name, assets[i].id);
    if (assets.size() > 10) names << QString("  … and %1 more").arg(assets.size() - 10);

    const auto answer = QMessageBox::question(
        this, "Delete Assets",
        QString("Permanently delete %1 asset%2?\n\n%3\n\n"
                "This removes all files and cannot be undone.")
            .arg(assets.size()).arg(assets.size() > 1 ? "s" : "").arg(names.join('\n')),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) return;

    m_preview->clear();
    try {
        for (const Asset &asset : assets) {
            Storage::deleteAssetFiles(asset);
            m_db->deleteAsset(asset.id);
        }
    } catch (const std::exception &e) {
        QMessageBox::critical(this, "Delete Failed", e.what());
    }
    m_toolbar->setAssetActionsEnabled(false);
    refreshAssets();
}

void MainWindow::onBackup() {
    bool ok = false;
    const QString password = QInputDialog::getText(this, "Backup Password",
        "Enter a password for the backup:", QLineEdit::Password, QString(), &ok);
    if (!ok || password.isEmpty()) return;

    const QString confirm = QInputDialog::getText(this, "Confirm Password",
        "Re-enter the password:", QLineEdit::Password, QString(), &ok);
    if (!ok) return;
    if (password != confirm) {
        QMessageBox::warning(this, "Backup", "Passwords do not match.");
        return;
    }

    const auto settings = Config::openSettings();
    const QString folder = settings->value("last_backup_path").toString();
    const QString suggested = folder.isEmpty() ? Backup::defaultName()
                                               : folder + "/" + Backup::defaultName();
    const QString dest = QFileDialog::getSaveFileName(this, "Save Backup", suggested,
                                                      "Zip Files (*.zip)");
    if (dest.isEmpty()) return;

    try {
        const QVector<AssetRecord> records = m_db->getAllAssetsWithFiles();
        runBusy(this, "Creating backup…", [&] { Backup::create(password, dest, records); });
        settings->setValue("last_backup_path", QFileInfo(dest).absolutePath());
        QMessageBox::information(this, "Backup Complete", "Backup saved to:\n" + dest);
    } catch (const std::exception &e) {
        QMessageBox::critical(this, "Backup Failed", e.what());
    }
}

void MainWindow::onRestore() {
    QMessageBox::warning(this, "Restore from Backup",
                         "Restoring will replace all current data with the backup contents.\n"
                         "Your existing data will be moved to a timestamped folder first.");
    const QString src = QFileDialog::getOpenFileName(this, "Select Backup File", QString(),
                                                     "Zip Files (*.zip)");
    if (src.isEmpty()) return;

    bool ok = false;
    const QString password = QInputDialog::getText(this, "Backup Password",
        "Enter the backup password:", QLineEdit::Password, QString(), &ok);
    if (!ok) return;

    try {
        QString staging;
        runBusy(this, "Checking backup…", [&] { staging = Backup::prepareRestore(password, src); });
        m_preview->clear();
        const QString aside = Backup::commitRestore(staging, *m_db);
        onDataReplaced();
        QString message = "Backup restored successfully.";
        if (!aside.isEmpty()) message += QString("\n\nYour previous data was kept in:\n%1").arg(aside);
        QMessageBox::information(this, "Restore Complete", message);
    } catch (const std::exception &e) {
        QMessageBox::critical(this, "Restore Failed",
                              QString("Could not restore backup:\n%1").arg(e.what()));
    }
}

void MainWindow::onDataReplaced() {
    m_preview->clear();
    m_toolbar->setAssetActionsEnabled(false);
    refreshAssets();
}

void MainWindow::onSync() {
    SyncDialog dialog(m_db, this);
    connect(&dialog, &SyncDialog::dataReplaced, this, &MainWindow::onDataReplaced);
    dialog.exec();
}

void MainWindow::offerToOpen(const QString &title, const QString &message, const QString &path) {
    if (QMessageBox::information(this, title, message, QMessageBox::Yes | QMessageBox::No)
        == QMessageBox::Yes)
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void MainWindow::onExportFiles() {
    const QString dest = QFileDialog::getExistingDirectory(this, "Choose Export Folder");
    if (dest.isEmpty()) return;
    try {
        QStringList failures;
        const int count = Storage::exportAssetFiles(m_db->getAllAssetsWithFiles(), dest, &failures);
        QString message = QString("Exported %1 file%2 to:\n%3").arg(count)
                              .arg(count == 1 ? "" : "s").arg(dest);
        if (!failures.isEmpty()) {
            message += QString("\n\n%1 could not be exported:\n%2").arg(failures.size())
                           .arg(failures.mid(0, 5).join('\n'));
        }
        offerToOpen("Export Complete", message + "\n\nOpen folder?", dest);
    } catch (const std::exception &e) {
        QMessageBox::critical(this, "Export Failed", e.what());
    }
}

void MainWindow::onExportCsv() {
    const QString dest = QFileDialog::getSaveFileName(this, "Export to CSV",
        CsvExport::defaultName(), "CSV Files (*.csv)");
    if (dest.isEmpty()) return;
    try {
        const QVector<Asset> assets = m_db->getAllAssets();
        CsvExport::write(assets, dest);
        offerToOpen("Export Complete",
                    QString("Exported %1 assets to:\n%2\n\nOpen file?").arg(assets.size()).arg(dest),
                    dest);
    } catch (const std::exception &e) {
        QMessageBox::critical(this, "Export Failed", e.what());
    }
}

void MainWindow::onPrint() {
    InsurerInfoDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) return;

    const QString dest = QFileDialog::getSaveFileName(this, "Save PDF Report",
        "Asset_Inventory_Report.pdf", "PDF Files (*.pdf)");
    if (dest.isEmpty()) return;
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        Report::generate(m_db->getAllAssetsWithFiles(), dest, dialog.insurerInfo());
        QApplication::restoreOverrideCursor();
        offerToOpen("Report Generated", QString("Report saved to:\n%1\n\nOpen file?").arg(dest),
                    dest);
    } catch (const std::exception &e) {
        QApplication::restoreOverrideCursor();
        QMessageBox::critical(this, "Report Failed", e.what());
    }
}

void MainWindow::onCheckForUpdates(bool announceNoUpdate) {
    if (!m_updateChecker) m_updateChecker = new UpdateChecker(Config::kAppVersion, this);
    disconnect(m_updateChecker, nullptr, this, nullptr);
    connect(m_updateChecker, &UpdateChecker::checkFinished, this,
            [this, announceNoUpdate](bool available, const QString &latest, const QString &url) {
        if (available) {
            const auto answer = QMessageBox::question(
                this, "Update Available",
                QString("Version %1 is available. You have %2.\n\nOpen the download page?")
                    .arg(latest, Config::kAppVersion));
            if (answer == QMessageBox::Yes) QDesktopServices::openUrl(QUrl(url));
        } else if (announceNoUpdate) {
            QMessageBox::information(this, "Check for Updates",
                latest.isEmpty() ? QStringLiteral("Could not reach the update server, or no "
                                                  "release has been published yet.")
                                 : QString("You have the latest version (%1).")
                                       .arg(Config::kAppVersion));
        }
    });
    m_updateChecker->check();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    const auto settings = Config::openSettings();
    if (settings->value("auto_backup_on_exit", false).toBool()) {
        const auto answer = QMessageBox::question(
            this, "Auto-backup", "Create a backup before exiting?",
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);
        if (answer == QMessageBox::Cancel) {
            event->ignore();
            return;
        }
        if (answer == QMessageBox::Yes) onBackup();
    }
    m_preview->clear();  // Flushes a pending notes edit.
    settings->setValue("geometry", saveGeometry());
    settings->setValue("splitter", m_splitter->saveState());
    QMainWindow::closeEvent(event);
}
