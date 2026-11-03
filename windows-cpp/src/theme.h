#pragma once
#include <QColor>

class QApplication;
class QPushButton;

// The one place the app's appearance is defined: palette, font and stylesheet.
// Widgets pick up styles through object names (see theme.cpp) rather than
// carrying their own colours.
namespace Theme {

inline const QColor kAccent("#1B2A3B");       // navy, shared with the PDF report
inline const QColor kAccentHover("#2C3E50");
inline const QColor kLink("#2F5D8C");
inline const QColor kText("#1F2933");
inline const QColor kMuted("#5B6675");
inline const QColor kBorder("#D9DEE5");
inline const QColor kWindow("#F4F6F8");
inline const QColor kSurface("#FFFFFF");
inline const QColor kSuccess("#2E7D4F");
inline const QColor kDanger("#B42318");

// Call once, after QApplication exists and before any window is built.
void apply(QApplication &app);

// The one emphasised action in a dialog: filled navy, and the Enter default.
void markPrimary(QPushButton *button);

} // namespace Theme
