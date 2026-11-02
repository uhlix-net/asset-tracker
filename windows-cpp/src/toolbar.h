#pragma once
#include <QWidget>

class QComboBox;
class QLineEdit;
class QPushButton;
class QTimer;

class Toolbar : public QWidget {
    Q_OBJECT
public:
    explicit Toolbar(QWidget *parent = nullptr);
    void setAssetActionsEnabled(bool enabled);

signals:
    void addClicked();
    void editClicked();
    void duplicateClicked();
    void deleteClicked();
    void backupClicked();
    void printClicked();
    void searchChanged(const QString &text);
    void categoryChanged(const QString &category);

private:
    QPushButton *m_edit;
    QPushButton *m_duplicate;
    QPushButton *m_delete;
    QComboBox *m_category;
    QLineEdit *m_search;
    QTimer *m_debounce;
};
