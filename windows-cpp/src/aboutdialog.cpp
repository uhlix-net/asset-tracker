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
    label->setStyleSheet("color: #555; font-family: monospace;");
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

} // namespace

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(QString("About %1").arg(Config::kAppName));
    setMinimumWidth(480);

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    auto *title = new QLabel(Config::kAppName);
    title->setStyleSheet("font-size: 20px; font-weight: bold;");
    title->setAlignment(Qt::AlignCenter);

    auto *version = new QLabel(QString("Version %1").arg(Config::kAppVersion));
    version->setAlignment(Qt::AlignCenter);
    version->setStyleSheet("color: #555;");

    auto *description = new QLabel("A personal asset tracking application for documenting\n"
                                   "household items for insurance purposes.");
    description->setAlignment(Qt::AlignCenter);
    description->setWordWrap(true);

    auto *line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("color: #d0d0d0;");

    auto *pathsLabel = new QLabel("Data Locations");
    pathsLabel->setStyleSheet("font-weight: bold; color: #333;");

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);
    form->setHorizontalSpacing(12);
    form->addRow("Database:", pathLabel(Config::dbPath()));
    form->addRow("Assets folder:", pathLabel(Config::assetsDir()));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);

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
