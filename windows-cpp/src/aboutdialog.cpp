#include "aboutdialog.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>

#include "config.h"

namespace {

QLabel *pathLabel(const QString &path) {
    auto *label = new QLabel(QDir::toNativeSeparators(path));
    label->setObjectName("mono");
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

} // namespace

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(QString("About %1").arg(Config::kAppName));
    setMinimumWidth(480);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 16);
    layout->setSpacing(8);

    auto *icon = new QLabel;
    icon->setPixmap(windowIcon().pixmap(56, 56));
    icon->setAlignment(Qt::AlignCenter);

    auto *title = new QLabel("Asset Tracker");
    title->setObjectName("title");
    title->setAlignment(Qt::AlignCenter);

    auto *version = new QLabel(QString("Version %1").arg(Config::kAppVersion));
    version->setObjectName("subtitle");
    version->setAlignment(Qt::AlignCenter);

    auto *description = new QLabel("A personal asset tracking application for documenting\n"
                                   "household items for insurance purposes.");
    description->setAlignment(Qt::AlignCenter);
    description->setWordWrap(true);

    auto *line = new QFrame;
    line->setObjectName("rule");
    line->setFrameShape(QFrame::HLine);

    auto *pathsLabel = new QLabel("DATA LOCATIONS");
    pathsLabel->setObjectName("section");

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);
    form->setHorizontalSpacing(12);
    form->addRow("Database:", pathLabel(Config::dbPath()));
    form->addRow("Assets folder:", pathLabel(Config::assetsDir()));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);

    layout->addWidget(icon);
    layout->addWidget(title);
    layout->addWidget(version);
    layout->addSpacing(4);
    layout->addWidget(description);
    layout->addSpacing(4);
    layout->addWidget(line);
    layout->addWidget(pathsLabel);
    layout->addLayout(form);
    layout->addSpacing(4);
    layout->addWidget(buttons);
}
