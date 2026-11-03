#pragma once
#include <QDialog>

#include "report.h"

class QLineEdit;

InsurerInfo loadInsurerInfo();

// Collects insurance company details before generating a report.
class InsurerInfoDialog : public QDialog {
    Q_OBJECT
public:
    explicit InsurerInfoDialog(QWidget *parent = nullptr);
    InsurerInfo insurerInfo() const;

private:
    void onAccept();

    QLineEdit *m_company;
    QLineEdit *m_policy;
    QLineEdit *m_claim;
    QLineEdit *m_lossDate;
};
