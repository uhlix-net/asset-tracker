#pragma once
#include <QDialog>

class ImageViewer : public QDialog {
    Q_OBJECT
public:
    ImageViewer(const QString &path, const QString &title, bool encrypted,
                QWidget *parent = nullptr);
};
