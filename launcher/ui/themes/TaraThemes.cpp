// SPDX-License-Identifier: GPL-3.0-only
#include "TaraThemes.h"

#include <QObject>
#include <QtGlobal>

#include "Application.h"
#include "settings/SettingsObject.h"

namespace {

struct Tokens {
    QString text, seltext, accent, disabled;
    QString panel, panelAlt, input, bar, popup;
    QString btn, btnHover, btnPressed, border, tooltip;
    int radius = 8;
    int radiusSmall = 5;
    QString extra;  // rules specific to one style
};

QString rgba(int r, int g, int b, int alpha)
{
    return QStringLiteral("rgba(%1,%2,%3,%4)").arg(r).arg(g).arg(b).arg(qBound(0, alpha, 255));
}

// Panel transparency chosen in Settings (percent of opacity, default 45).
int panelAlpha()
{
    const QVariant stored = APPLICATION->settings()->get("TaraPanelOpacity");
    const int percent = qBound(0, stored.isValid() ? stored.toInt() : 45, 100);
    return percent * 255 / 100;
}

QString build(const Tokens& t)
{
    QString css = QStringLiteral(R"(
QToolTip { color: %TEXT%; background-color: %TOOLTIP%; border: 1px solid %ACCENT%; padding: 4px; }
QListView, QTreeView, QTableView, QListWidget, QTreeWidget, QTableWidget, QTextEdit, QPlainTextEdit, QTextBrowser {
    background-color: %PANEL%; alternate-background-color: %PANELALT%; border: 1px solid %BORDER%; border-radius: %RADIUS%px;
    selection-background-color: %ACCENT%; selection-color: %SELTEXT%; color: %TEXT%; }
QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
    background-color: %INPUT%; border: 1px solid %BORDER%; border-radius: %RADIUSS%px; padding: 3px 6px; color: %TEXT%;
    selection-background-color: %ACCENT%; selection-color: %SELTEXT%; }
QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus { border: 1px solid %ACCENT%; }
QComboBox QAbstractItemView { background-color: %POPUP%; border: 1px solid %BORDER%; selection-background-color: %ACCENT%; selection-color: %SELTEXT%; }
QPushButton, QToolButton { background: %BTN%; border: 1px solid %BORDER%; border-radius: %RADIUS%px; padding: 5px 12px; color: %TEXT%; }
QToolButton { padding: 4px 8px; }
QPushButton:hover, QToolButton:hover { background: %BTNHOVER%; border: 1px solid %ACCENT%; }
QPushButton:pressed, QToolButton:pressed, QToolButton:checked { background: %BTNPRESSED%; }
QPushButton:disabled, QToolButton:disabled { color: %DISABLED%; }
QToolBar { background: %BAR%; border: none; spacing: 4px; padding: 3px; }
QMenuBar { background: %BAR%; color: %TEXT%; }
QMenuBar::item { background: transparent; padding: 4px 10px; }
QMenuBar::item:selected { background: %BTNHOVER%; border-radius: %RADIUSS%px; }
QMenu { background-color: %POPUP%; border: 1px solid %BORDER%; padding: 4px; color: %TEXT%; }
QMenu::item { padding: 5px 24px; border-radius: %RADIUSS%px; }
QMenu::item:selected { background-color: %ACCENT%; color: %SELTEXT%; }
QStatusBar { background: %BAR%; color: %TEXT%; }
QTabWidget::pane { border: 1px solid %BORDER%; background-color: %PANEL%; border-radius: %RADIUS%px; top: -1px; }
QTabBar::tab { background: %BTN%; border: 1px solid %BORDER%; border-bottom: none; padding: 5px 14px; margin-right: 2px;
    border-top-left-radius: %RADIUSS%px; border-top-right-radius: %RADIUSS%px; color: %TEXT%; }
QTabBar::tab:selected { background: %PANEL%; border-color: %ACCENT%; }
QHeaderView::section { background: %BAR%; border: none; border-right: 1px solid %BORDER%; padding: 4px 8px; color: %TEXT%; }
QGroupBox { border: 1px solid %BORDER%; border-radius: %RADIUS%px; margin-top: 10px; padding-top: 8px; }
QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }
QScrollBar:vertical { background: transparent; width: 12px; margin: 0; }
QScrollBar::handle:vertical { background: %BTNHOVER%; border-radius: 5px; min-height: 24px; margin: 2px; }
QScrollBar:horizontal { background: transparent; height: 12px; margin: 0; }
QScrollBar::handle:horizontal { background: %BTNHOVER%; border-radius: 5px; min-width: 24px; margin: 2px; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
QProgressBar { border: 1px solid %BORDER%; border-radius: %RADIUSS%px; background: %INPUT%; text-align: center; color: %TEXT%; }
QProgressBar::chunk { background-color: %ACCENT%; border-radius: %RADIUSS%px; }
QSlider::groove:horizontal { height: 6px; background: %INPUT%; border-radius: 3px; }
QSlider::handle:horizontal { background: %ACCENT%; width: 14px; margin: -5px 0; border-radius: 7px; }
)");
    css += t.extra;
    css.replace(QStringLiteral("%TEXT%"), t.text);
    css.replace(QStringLiteral("%SELTEXT%"), t.seltext);
    css.replace(QStringLiteral("%ACCENT%"), t.accent);
    css.replace(QStringLiteral("%DISABLED%"), t.disabled);
    css.replace(QStringLiteral("%PANELALT%"), t.panelAlt);
    css.replace(QStringLiteral("%PANEL%"), t.panel);
    css.replace(QStringLiteral("%INPUT%"), t.input);
    css.replace(QStringLiteral("%BAR%"), t.bar);
    css.replace(QStringLiteral("%POPUP%"), t.popup);
    css.replace(QStringLiteral("%BTNHOVER%"), t.btnHover);
    css.replace(QStringLiteral("%BTNPRESSED%"), t.btnPressed);
    css.replace(QStringLiteral("%BTN%"), t.btn);
    css.replace(QStringLiteral("%BORDER%"), t.border);
    css.replace(QStringLiteral("%TOOLTIP%"), t.tooltip);
    css.replace(QStringLiteral("%RADIUSS%"), QString::number(t.radiusSmall));
    css.replace(QStringLiteral("%RADIUS%"), QString::number(t.radius));
    return css;
}

}  // namespace

QString TaraTheme::id()
{
    switch (m_kind) {
        case Kind::Glass: return QStringLiteral("tara-glass");
        case Kind::Smooth: return QStringLiteral("tara-smooth");
        case Kind::Neon: return QStringLiteral("tara-neon");
        case Kind::Minimal: return QStringLiteral("tara-minimal");
    }
    return QString();
}

QString TaraTheme::name()
{
    switch (m_kind) {
        case Kind::Glass: return QObject::tr("Стекло");
        case Kind::Smooth: return QObject::tr("Гладкий");
        case Kind::Neon: return QObject::tr("Неон");
        case Kind::Minimal: return QObject::tr("Минимализм");
    }
    return QString();
}

QString TaraTheme::tooltip()
{
    switch (m_kind) {
        case Kind::Glass: return QObject::tr("Полупрозрачные панели и кнопки. Лучше всего выглядит со своей картинкой на фоне.");
        case Kind::Smooth: return QObject::tr("Светлая тема с мягкими скруглёнными элементами.");
        case Kind::Neon: return QObject::tr("Тёмная тема с яркими светящимися акцентами.");
        case Kind::Minimal: return QObject::tr("Плоский вид без рамок и лишних деталей.");
    }
    return QString();
}

QPalette TaraTheme::colorScheme()
{
    QPalette p;
    auto set = [&p](QPalette::ColorRole role, const QColor& color) { p.setColor(role, color); };
    switch (m_kind) {
        case Kind::Glass:
            set(QPalette::Window, QColor(18, 22, 32));
            set(QPalette::WindowText, Qt::white);
            set(QPalette::Base, QColor(14, 18, 26));
            set(QPalette::AlternateBase, QColor(24, 29, 40));
            set(QPalette::Text, Qt::white);
            set(QPalette::Button, QColor(40, 48, 64));
            set(QPalette::ButtonText, Qt::white);
            set(QPalette::ToolTipBase, QColor(24, 28, 40));
            set(QPalette::ToolTipText, Qt::white);
            set(QPalette::Highlight, QColor(108, 184, 255));
            set(QPalette::HighlightedText, QColor(8, 18, 31));
            set(QPalette::Link, QColor(108, 184, 255));
            set(QPalette::PlaceholderText, QColor(150, 160, 176));
            break;
        case Kind::Smooth:
            set(QPalette::Window, QColor(236, 240, 247));
            set(QPalette::WindowText, QColor(40, 48, 64));
            set(QPalette::Base, QColor(251, 252, 255));
            set(QPalette::AlternateBase, QColor(241, 244, 251));
            set(QPalette::Text, QColor(40, 48, 64));
            set(QPalette::Button, QColor(255, 255, 255));
            set(QPalette::ButtonText, QColor(40, 48, 64));
            set(QPalette::ToolTipBase, Qt::white);
            set(QPalette::ToolTipText, QColor(40, 48, 64));
            set(QPalette::Highlight, QColor(94, 129, 244));
            set(QPalette::HighlightedText, Qt::white);
            set(QPalette::Link, QColor(60, 100, 220));
            set(QPalette::PlaceholderText, QColor(140, 150, 170));
            break;
        case Kind::Neon:
            set(QPalette::Window, QColor(10, 10, 18));
            set(QPalette::WindowText, QColor(220, 230, 255));
            set(QPalette::Base, QColor(10, 10, 20));
            set(QPalette::AlternateBase, QColor(18, 18, 34));
            set(QPalette::Text, QColor(220, 230, 255));
            set(QPalette::Button, QColor(18, 18, 42));
            set(QPalette::ButtonText, QColor(220, 230, 255));
            set(QPalette::ToolTipBase, QColor(14, 14, 32));
            set(QPalette::ToolTipText, QColor(220, 230, 255));
            set(QPalette::Highlight, QColor(0, 240, 255));
            set(QPalette::HighlightedText, Qt::black);
            set(QPalette::Link, QColor(255, 43, 214));
            set(QPalette::PlaceholderText, QColor(110, 120, 160));
            break;
        case Kind::Minimal:
            set(QPalette::Window, QColor(30, 30, 30));
            set(QPalette::WindowText, QColor(228, 228, 228));
            set(QPalette::Base, QColor(38, 38, 38));
            set(QPalette::AlternateBase, QColor(43, 43, 43));
            set(QPalette::Text, QColor(228, 228, 228));
            set(QPalette::Button, QColor(51, 51, 51));
            set(QPalette::ButtonText, QColor(228, 228, 228));
            set(QPalette::ToolTipBase, QColor(43, 43, 43));
            set(QPalette::ToolTipText, QColor(228, 228, 228));
            set(QPalette::Highlight, QColor(208, 208, 208));
            set(QPalette::HighlightedText, QColor(17, 17, 17));
            set(QPalette::Link, QColor(160, 190, 255));
            set(QPalette::PlaceholderText, QColor(130, 130, 130));
            break;
    }
    return fadeInactive(p, fadeAmount(), fadeColor());
}

QColor TaraTheme::fadeColor()
{
    return colorScheme().color(QPalette::Active, QPalette::Window);
}

QString TaraTheme::appStyleSheet()
{
    Tokens t;
    switch (m_kind) {
        case Kind::Glass: {
            const int a = panelAlpha();
            t.text = "#ffffff";
            t.seltext = "#08121f";
            t.accent = "#6cb8ff";
            t.disabled = rgba(255, 255, 255, 90);
            t.panel = rgba(16, 20, 30, a);
            t.panelAlt = rgba(255, 255, 255, 12);
            t.input = rgba(0, 0, 0, qMin(255, a + 30));
            t.bar = rgba(0, 0, 0, qMin(255, a + 20));
            t.popup = rgba(24, 28, 40, 238);
            t.btn = rgba(255, 255, 255, 26);
            t.btnHover = rgba(255, 255, 255, 58);
            t.btnPressed = rgba(255, 255, 255, 84);
            t.border = rgba(255, 255, 255, 64);
            t.tooltip = rgba(24, 28, 40, 240);
            t.radius = 10;
            t.radiusSmall = 6;
            break;
        }
        case Kind::Smooth:
            t.text = "#283040";
            t.seltext = "#ffffff";
            t.accent = "#5e81f4";
            t.disabled = "#9aa4b8";
            t.panel = "#fbfcff";
            t.panelAlt = "#f1f4fb";
            t.input = "#ffffff";
            t.bar = "#e4e9f4";
            t.popup = "#ffffff";
            t.btn = "qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #ffffff, stop:1 #e9eefa)";
            t.btnHover = "qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #ffffff, stop:1 #dde6fb)";
            t.btnPressed = "#d3ddf6";
            t.border = "#cdd6ea";
            t.tooltip = "#ffffff";
            t.radius = 12;
            t.radiusSmall = 8;
            break;
        case Kind::Neon:
            t.text = "#dce6ff";
            t.seltext = "#000000";
            t.accent = "#00f0ff";
            t.disabled = "#5a6080";
            t.panel = "#0c0c1a";
            t.panelAlt = "#121226";
            t.input = "#0a0a14";
            t.bar = "#0c0c1a";
            t.popup = "#0e0e20";
            t.btn = "#12122a";
            t.btnHover = "#1a1a40";
            t.btnPressed = "#22225a";
            t.border = "#2c2c66";
            t.tooltip = "#0e0e20";
            t.radius = 6;
            t.radiusSmall = 4;
            t.extra = QStringLiteral(
                "QPushButton:hover, QToolButton:hover { border: 1px solid #ff2bd6; }\n"
                "QLineEdit:focus, QComboBox:focus, QSpinBox:focus { border: 1px solid #ff2bd6; }\n"
                "QTabBar::tab:selected { border-color: #00f0ff; border-bottom: 2px solid #ff2bd6; }\n"
                "QProgressBar::chunk { background-color: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #00f0ff, stop:1 #ff2bd6); }\n");
            break;
        case Kind::Minimal:
            t.text = "#e4e4e4";
            t.seltext = "#111111";
            t.accent = "#d0d0d0";
            t.disabled = "#777777";
            t.panel = "#262626";
            t.panelAlt = "#2b2b2b";
            t.input = "#2b2b2b";
            t.bar = "#1e1e1e";
            t.popup = "#2b2b2b";
            t.btn = "#333333";
            t.btnHover = "#3d3d3d";
            t.btnPressed = "#4a4a4a";
            t.border = "transparent";
            t.tooltip = "#2b2b2b";
            t.radius = 0;
            t.radiusSmall = 0;
            t.extra = QStringLiteral("QPushButton:hover, QToolButton:hover { border: 1px solid transparent; }\n");
            break;
    }
    return build(t);
}
