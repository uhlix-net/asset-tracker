#include "theme.h"

#include <QApplication>
#include <QDir>
#include <QFont>
#include <QPalette>
#include <QPushButton>
#include <QStyle>
#include <QStyleHints>

#include "icons.h"

namespace {

QString styleSheet() {
    const QString down = QDir::fromNativeSeparators(Icons::chevronFile(false, Theme::kMuted));
    const QString up = QDir::fromNativeSeparators(Icons::chevronFile(true, Theme::kMuted));

    QString qss = R"(
QMainWindow, QDialog { background: @window; }
QToolTip { color: @text; background: @surface; border: 1px solid @border; padding: 4px; }

/* Header bar above the list */
QWidget#toolbar { background: @surface; border-bottom: 1px solid @border; }

QPushButton {
    color: @text; background: @surface;
    border: 1px solid @border; border-radius: 5px;
    padding: 5px 12px; min-height: 18px;
}
QPushButton:hover { background: #EEF2F6; border-color: #C3CBD5; }
QPushButton:pressed { background: #E3E9EF; }
QPushButton:disabled { color: #A6AFBA; background: #F7F8FA; border-color: #E4E8ED; }
QPushButton:default { border-color: @accent; }

QPushButton#primary, QPushButton[primary="true"] { color: white; background: @accent; border-color: @accent; }
QPushButton#primary:hover, QPushButton[primary="true"]:hover { background: @accentHover; border-color: @accentHover; }
QPushButton#primary:pressed, QPushButton[primary="true"]:pressed { background: #111C28; }
QPushButton#primary:disabled, QPushButton[primary="true"]:disabled { color: #DDE2E8; background: #8A96A3; border-color: #8A96A3; }

QPushButton[danger="true"] { color: #B42318; }
QPushButton[danger="true"]:hover { background: #FDF0EE; border-color: #F1B9B2; }
QPushButton[danger="true"]:disabled { color: #A6AFBA; }

QGroupBox {
    color: @text; font-weight: 600; background: @surface;
    border: 1px solid @border; border-radius: 6px;
    margin-top: 14px; padding: 12px 10px 8px 10px;
}
QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }
QRadioButton { color: @text; spacing: 6px; }

QPushButton#flat { border-color: transparent; background: transparent; }
QPushButton#flat:hover { background: #EEF2F6; border-color: transparent; }
QPushButton#flat:disabled { background: transparent; border-color: transparent; }

QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QDateEdit {
    color: @text; background: @surface;
    border: 1px solid @border; border-radius: 5px;
    padding: 4px 8px; min-height: 18px;
    selection-background-color: #C9D8EA; selection-color: @text;
}
QLineEdit:focus, QComboBox:focus, QDoubleSpinBox:focus, QDateEdit:focus,
QTextEdit:focus, QPlainTextEdit:focus { border-color: @link; }
QLineEdit:disabled, QComboBox:disabled, QDoubleSpinBox:disabled, QDateEdit:disabled {
    color: #A6AFBA; background: #F7F8FA;
}
QComboBox::drop-down, QDateEdit::drop-down { border: none; width: 22px; }
QComboBox::down-arrow, QDateEdit::down-arrow { image: url(@down); width: 12px; height: 12px; }
QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { border: none; width: 18px; }
QDoubleSpinBox::up-arrow { image: url(@up); width: 10px; height: 10px; }
QDoubleSpinBox::down-arrow { image: url(@down); width: 10px; height: 10px; }
QComboBox QAbstractItemView {
    background: @surface; border: 1px solid @border;
    selection-background-color: #E3EBF4; selection-color: @text; outline: 0;
}

QTextEdit, QPlainTextEdit, QListWidget {
    color: @text; background: @surface;
    border: 1px solid @border; border-radius: 5px; padding: 2px;
}
QListWidget::item { padding: 3px 4px; }
QListWidget::item:selected { background: #E3EBF4; color: @text; }

QCheckBox { color: @text; spacing: 6px; }

/* Asset table */
QTableWidget {
    background: @surface; border: none;
    alternate-background-color: #F8FAFB;
    selection-background-color: #DCE6F1; selection-color: @text;
    outline: 0;
}
QTableWidget::item { padding: 0 6px; border-bottom: 1px solid #EDF0F3; }
QHeaderView::section {
    color: @muted; background: @surface; font-weight: 600;
    border: none; border-bottom: 1px solid @border;
    padding: 6px 6px;
}
QHeaderView::section:hover { color: @text; }

QSplitter::handle { background: @border; }
QSplitter::handle:horizontal { width: 1px; }

QScrollArea { background: transparent; border: none; }
QScrollArea > QWidget > QWidget { background: transparent; }
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle { background: #C5CDD6; border-radius: 3px; min-height: 24px; min-width: 24px; }
QScrollBar::handle:hover { background: #A9B3BF; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: none; }

QStatusBar { color: @muted; background: @surface; border-top: 1px solid @border; }
QStatusBar::item { border: none; }

QMenuBar { background: @surface; border-bottom: 1px solid @border; }
QMenuBar::item { padding: 4px 10px; background: transparent; }
QMenuBar::item:selected { background: #EEF2F6; border-radius: 4px; }
QMenu { background: @surface; border: 1px solid @border; padding: 4px; }
QMenu::item { padding: 5px 24px 5px 20px; border-radius: 4px; }
QMenu::item:selected { background: #E3EBF4; color: @text; }
QMenu::separator { height: 1px; background: @border; margin: 4px 8px; }

QTabWidget::pane { border: 1px solid @border; border-radius: 5px; background: @surface; top: -1px; }
QTabBar::tab {
    color: @muted; background: transparent; padding: 6px 14px;
    border: 1px solid transparent; border-bottom: none;
}
QTabBar::tab:selected {
    color: @text; background: @surface; border-color: @border;
    border-top-left-radius: 5px; border-top-right-radius: 5px;
}
QProgressBar { border: 1px solid @border; border-radius: 5px; background: @surface; height: 8px; }
QProgressBar::chunk { background: @accent; border-radius: 4px; }

/* Text roles */
QLabel#title { color: @text; font-size: 15pt; font-weight: 600; }
QLabel#subtitle { color: @muted; }
QLabel#section {
    color: @muted; font-size: 8pt; font-weight: 700; letter-spacing: 1px;
    padding-top: 10px;
}
QLabel#fieldLabel { color: @muted; }
QLabel#hint { color: @muted; font-size: 8.5pt; }
QLabel#mono { color: @muted; font-family: Consolas, monospace; }
QLabel#emptyState { color: #8A96A3; font-size: 11pt; }
QLabel#photoTile { background: @surface; border: 1px solid @border; border-radius: 6px; }
QLabel#photoTile:hover { border-color: @link; }
QLabel#version { color: @accent; font-size: 11pt; font-weight: 600; }

/* Detail panel */
QWidget#previewPanel { background: @surface; }
QFrame#rule { color: @border; background: @border; max-height: 1px; border: none; }
)";
    const struct { const char *name; QColor color; } tokens[] = {
        {"@accentHover", Theme::kAccentHover}, {"@accent", Theme::kAccent},
        {"@link", Theme::kLink}, {"@text", Theme::kText}, {"@muted", Theme::kMuted},
        {"@border", Theme::kBorder}, {"@window", Theme::kWindow},
        {"@surface", Theme::kSurface},
    };
    for (const auto &token : tokens)
        qss.replace(token.name, token.color.name());
    qss.replace("@down", down).replace("@up", up);
    return qss;
}

} // namespace

void Theme::markPrimary(QPushButton *button, bool primary) {
    button->setProperty("primary", primary);
    button->setDefault(primary);
    // A button box has already polished its buttons by the time we get them, and
    // a stylesheet is only matched against properties at polish time.
    button->style()->unpolish(button);
    button->style()->polish(button);
}

void Theme::apply(QApplication &app) {
    app.setStyle("Fusion");
    app.styleHints()->setColorScheme(Qt::ColorScheme::Light);

    QFont font("Segoe UI");
    font.setPointSizeF(9.5);
    app.setFont(font);

    QPalette palette = app.palette();
    palette.setColor(QPalette::Window, kWindow);
    palette.setColor(QPalette::Base, kSurface);
    palette.setColor(QPalette::AlternateBase, QColor("#F8FAFB"));
    palette.setColor(QPalette::Text, kText);
    palette.setColor(QPalette::WindowText, kText);
    palette.setColor(QPalette::ButtonText, kText);
    palette.setColor(QPalette::Button, kSurface);
    palette.setColor(QPalette::Highlight, QColor("#DCE6F1"));
    palette.setColor(QPalette::HighlightedText, kText);
    palette.setColor(QPalette::Link, kLink);
    palette.setColor(QPalette::PlaceholderText, QColor("#8A96A3"));
    app.setPalette(palette);

    app.setStyleSheet(styleSheet());
}
