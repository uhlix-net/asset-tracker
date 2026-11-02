#include "insurerinfodialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

#include "config.h"

InsurerInfo loadInsurerInfo() {
    const auto settings = Config::openSettings();
    InsurerInfo info;
    info.company = settings->value("insurer/company").toString();
    info.policyNumber = settings->value("insurer/policy_number").toString();
    info.claimNumber = settings->value("insurer/claim_number").toString();
    info.dateOfLoss = settings->value("insurer/date_of_loss").toString();
    return info;
}

InsurerInfoDialog::InsurerInfoDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("Insurance Claim Information");
    setMinimumWidth(420);

    auto *layout = new QVBoxLayout(this);

    auto *note = new QLabel("This information will appear on the report cover page.\n"
                            "All fields are optional — leave blank if unknown.");
    note->setStyleSheet("color: #555; font-size: 10px;");
    note->setWordWrap(true);
    layout->addWidget(note);

    const InsurerInfo saved = loadInsurerInfo();
    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);
    const auto addRow = [form](const QString &label, const QString &value, const QString &hint) {
        auto *edit = new QLineEdit(value);
        edit->setPlaceholderText(hint);
        form->addRow(label, edit);
        return edit;
    };
    m_company = addRow("Insurance Company:", saved.company, "e.g. State Farm Insurance");
    m_policy = addRow("Policy Number:", saved.policyNumber, "e.g. PO-1234567");
    m_claim = addRow("Claim Number:", saved.claimNumber, "e.g. CLM-2026-00001");
    m_lossDate = addRow("Date of Loss:", saved.dateOfLoss, "e.g. May 1, 2026");
    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &InsurerInfoDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

InsurerInfo InsurerInfoDialog::insurerInfo() const {
    return {m_company->text().trimmed(), m_policy->text().trimmed(),
            m_claim->text().trimmed(), m_lossDate->text().trimmed()};
}

void InsurerInfoDialog::onAccept() {
    const InsurerInfo info = insurerInfo();
    const auto settings = Config::openSettings();
    settings->setValue("insurer/company", info.company);
    settings->setValue("insurer/policy_number", info.policyNumber);
    settings->setValue("insurer/claim_number", info.claimNumber);
    settings->setValue("insurer/date_of_loss", info.dateOfLoss);
    accept();
}
