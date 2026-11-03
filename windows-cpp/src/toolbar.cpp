#include "toolbar.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>

#include "config.h"
#include "icons.h"
#include "theme.h"

namespace {

QPushButton *makeButton(const QString &text, const QIcon &icon, const char *role = "flat",
                        bool enabled = true) {
    auto *button = new QPushButton(icon, text);
    button->setObjectName(role);
    button->setIconSize(QSize(16, 16));
    button->setCursor(Qt::PointingHandCursor);
    button->setEnabled(enabled);
    return button;
}

QFrame *divider() {
    auto *line = new QFrame;
    line->setObjectName("rule");
    line->setFixedSize(1, 22);
    return line;
}

} // namespace

Toolbar::Toolbar(QWidget *parent) : QWidget(parent) {
    setObjectName("toolbar");
    setAttribute(Qt::WA_StyledBackground);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(4);

    const QColor ink = Theme::kText;
    auto *add = makeButton("Add Asset", Icons::add(Qt::white), "primary");
    m_edit = makeButton("Edit", Icons::edit(ink), "flat", false);
    m_duplicate = makeButton("Duplicate", Icons::duplicate(ink), "flat", false);
    m_delete = makeButton("Delete", Icons::remove(ink), "flat", false);
    auto *backup = makeButton("Backup", Icons::backup(ink));
    auto *print = makeButton("Print Report", Icons::print(ink));

    connect(add, &QPushButton::clicked, this, &Toolbar::addClicked);
    connect(m_edit, &QPushButton::clicked, this, &Toolbar::editClicked);
    connect(m_duplicate, &QPushButton::clicked, this, &Toolbar::duplicateClicked);
    connect(m_delete, &QPushButton::clicked, this, &Toolbar::deleteClicked);
    connect(backup, &QPushButton::clicked, this, &Toolbar::backupClicked);
    connect(print, &QPushButton::clicked, this, &Toolbar::printClicked);

    m_category = new QComboBox;
    m_category->setMinimumWidth(170);
    m_category->addItem("All categories", QString());
    for (const QString &category : Config::categories())
        m_category->addItem(category, category);
    connect(m_category, &QComboBox::currentIndexChanged, this, [this] {
        emit categoryChanged(m_category->currentData().toString());
    });

    m_search = new QLineEdit;
    m_search->setPlaceholderText("Search assets");
    m_search->setClearButtonEnabled(true);
    m_search->setFixedWidth(240);
    m_search->addAction(Icons::search(Theme::kMuted), QLineEdit::LeadingPosition);

    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(300);
    connect(m_debounce, &QTimer::timeout, this, [this] {
        emit searchChanged(m_search->text().trimmed());
    });
    connect(m_search, &QLineEdit::textChanged, m_debounce, qOverload<>(&QTimer::start));

    layout->addWidget(add);
    layout->addSpacing(8);
    layout->addWidget(m_edit);
    layout->addWidget(m_duplicate);
    layout->addWidget(m_delete);
    layout->addSpacing(6);
    layout->addWidget(divider());
    layout->addSpacing(6);
    layout->addWidget(backup);
    layout->addWidget(print);
    layout->addStretch();
    layout->addWidget(m_category);
    layout->addSpacing(6);
    layout->addWidget(m_search);
}

void Toolbar::setAssetActionsEnabled(bool enabled) {
    m_edit->setEnabled(enabled);
    m_duplicate->setEnabled(enabled);
    m_delete->setEnabled(enabled);
}
