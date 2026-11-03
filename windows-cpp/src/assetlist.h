#pragma once
#include <QTableWidget>
#include <QVector>
#include <optional>

#include "models.h"

class Database;
class QLabel;

class AssetList : public QTableWidget {
    Q_OBJECT
public:
    explicit AssetList(Database *db, QWidget *parent = nullptr);

    void loadAssets(const QVector<Asset> &assets);
    std::optional<Asset> selectedAsset() const;
    QVector<Asset> selectedAssets() const;

signals:
    void assetSelected(const Asset &asset);
    void selectionCleared();
    void editRequested(const Asset &asset);
    void duplicateRequested(const Asset &asset);
    void deleteRequested(const QVector<Asset> &assets);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void populateRow(int row, const Asset &asset);
    void onSelectionChanged();
    void showContextMenu(const QPoint &pos);

    Database *m_db;
    QVector<Asset> m_assets;
    QLabel *m_emptyLabel;
};
