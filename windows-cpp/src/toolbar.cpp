#include "toolbar.h"

#include <QApplication>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStyle>
#include <QTimer>

#include "config.h"

namespace {

QPushButton *makeButton(const QString &text, QStyle::StandardPixmap icon, bool enabled = true) {
    auto *button = new QPushButton(text);
    button->setIcon(QApplication::style()->standardIcon(icon));
    button->setIconSize(QSize(16, 16));
    button->setFixedHeight(32);
    button->setEnabled(enabled);
    return button;
}

} // namespace

Toolbar::Toolbar(QWidget *parent) : QWidget(parent) {
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(6);

    auto *add = makeButton("Add Asset", QStyle::SP_FileDialogNewFolder);
    m_edit = makeButton("Edit", QStyle::SP_FileDialogDetailedView, false);
    m_duplicate = makeButton("Duplicate", QStyle::SP_FileLinkIcon, false);
    m_delete = makeButton("Delete", QStyle::SP_TrashIcon, false);
    auto *backup = makeButton("Backup", QStyle::SP_DialogSaveButton);
    auto *print = makeButton("Print Report", QStyle::SP_FileIcon);

    connect(add, &QPushButton::clicked, this, &Toolbar::addClicked);
    connect(m_edit, &QPushButton::clicked, this, &Toolbar::editClicked);
    connect(m_duplicate, &QPushButton::clicked, this, &Toolbar::duplicateClicked);
    connect(m_delete, &QPushButton::clicked, this, &Toolbar::deleteClicked);
    connect(backup, &QPushButton::clicked, this, &Toolbar::backupClicked);
    connect(print, &QPushButton::clicked, this, &Toolbar::printClicked);

    m_category = new QComboBox;
    m_category->setFixedHeight(32);
    m_category->setMinimumWidth(140);
    m_category->addItem("All Categories", QString());
    for (const QString &category : Config::categories())
        m_category->addItem(category, category);
    connect(m_category, &QComboBox::currentIndexChanged, this, [this] {
        emit categoryChanged(m_category->currentData().toString());
    });

    m_search = new QLineEdit;
    m_search->setPlaceholderText("Search by name...");
    m_search->setClearButtonEnabled(true);
    m_search->setFixedHeight(32);
    m_search->setMaximumWidth(240);

    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(300);
    connect(m_debounce, &QTimer::timeout, this, [this] {
        emit searchChanged(m_search->text().trimmed());
    });
    connect(m_search, &QLineEdit::textChanged, m_debounce, qOverload<>(&QTimer::start));

    for (QWidget *button : {static_cast<QWidget *>(add), static_cast<QWidget *>(m_edit),
                            static_cast<QWidget *>(m_duplicate), static_cast<QWidget *>(m_delete),
                            static_cast<QWidget *>(backup), static_cast<QWidget *>(print)})
        layout->addWidget(button);
    layout->addStretch();
    layout->addWidget(m_category);
    layout->addWidget(new QLabel("Search:"));
    layout->addWidget(m_search);
}

void Toolbar::setAssetActionsEnabled(bool enabled) {
    m_edit->setEnabled(enabled);
    m_duplicate->setEnabled(enabled);
    m_delete->setEnabled(enabled);
}
