#pragma once
#include <QString>
#include <functional>

class QWidget;

// Runs work on a worker thread behind a modal "please wait" dialog, so the window
// stays painted. Rethrows anything work threw, on the UI thread.
void runBusy(QWidget *parent, const QString &label, const std::function<void()> &work);
