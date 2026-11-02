#include "imageviewer.h"

#include <QDialogButtonBox>
#include <QFileInfo>
#include <QLabel>
#include <QPixmap>
#include <QScreen>
#include <QScrollArea>
#include <QVBoxLayout>

#include "storage.h"

ImageViewer::ImageViewer(const QString &path, const QString &title, bool encrypted, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(title.isEmpty() ? QFileInfo(path).fileName() : title);
    resize(800, 600);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto *label = new QLabel;
    label->setAlignment(Qt::AlignCenter);

    auto *scroll = new QScrollArea;
    scroll->setAlignment(Qt::AlignCenter);
    scroll->setWidget(label);
    layout->addWidget(scroll);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    QImage image;
    try {
        image = Storage::loadImage(path, encrypted);
    } catch (const std::exception &) {}

    if (image.isNull()) {
        label->setText("Cannot display: " + QFileInfo(path).fileName());
        label->adjustSize();
        return;
    }
    const QRect screenRect = screen()->availableGeometry();
    const QPixmap scaled = QPixmap::fromImage(image).scaled(
        qRound(screenRect.width() * 0.85), qRound(screenRect.height() * 0.80),
        Qt::KeepAspectRatio, Qt::SmoothTransformation);
    label->setPixmap(scaled);
    label->resize(scaled.size());
}
