// SPDX-License-Identifier: GPL-3.0-only
#include "HelpIcon.h"

#include <QCursor>
#include <QHBoxLayout>
#include <QToolTip>

HelpIcon::HelpIcon(const QString& helpText, QWidget* parent) : QLabel(QStringLiteral("?"), parent)
{
    setFixedSize(16, 16);
    setAlignment(Qt::AlignCenter);
    setCursor(Qt::WhatsThisCursor);
    // colours come from the current palette, so every theme (light, dark, glass...) looks right
    setStyleSheet(QStringLiteral(
        "QLabel { border: 1px solid palette(mid); border-radius: 8px; font-size: 10px; font-weight: bold; color: palette(text); }"
        "QLabel:hover { background: palette(highlight); color: palette(highlighted-text); border-color: palette(highlight); }"));
    setHelpText(helpText);
}

void HelpIcon::setHelpText(const QString& text)
{
    // an <html> prefix makes Qt treat the tooltip as rich text, which wraps long lines
    m_html = QStringLiteral("<html><body>%1</body></html>").arg(text.toHtmlEscaped().replace('\n', QStringLiteral("<br>")));
    setToolTip(m_html);
    setToolTipDuration(60000);
}

QWidget* HelpIcon::wrap(QWidget* field, const QString& helpText, QWidget* parent)
{
    auto* box = new QWidget(parent);
    auto* layout = new QHBoxLayout(box);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    field->setParent(box);
    layout->addWidget(field, 1);
    layout->addWidget(new HelpIcon(helpText, box), 0, Qt::AlignVCenter);
    return box;
}

void HelpIcon::showPopup()
{
    QToolTip::showText(mapToGlobal(QPoint(width(), height())), m_html, this, QRect(), 60000);
}

void HelpIcon::enterEvent(QEnterEvent* event)
{
    showPopup();
    QLabel::enterEvent(event);
}

void HelpIcon::leaveEvent(QEvent* event)
{
    QToolTip::hideText();
    QLabel::leaveEvent(event);
}

void HelpIcon::mousePressEvent(QMouseEvent* event)
{
    showPopup();
    QLabel::mousePressEvent(event);
}
