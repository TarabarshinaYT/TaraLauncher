// SPDX-License-Identifier: GPL-3.0-only
#include "MotdPreview.h"

#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QRandomGenerator>

namespace Motd {

QColor colorForCode(QChar code)
{
    switch (code.toLower().unicode()) {
        case '0': return QColor(0x00, 0x00, 0x00);
        case '1': return QColor(0x00, 0x00, 0xAA);
        case '2': return QColor(0x00, 0xAA, 0x00);
        case '3': return QColor(0x00, 0xAA, 0xAA);
        case '4': return QColor(0xAA, 0x00, 0x00);
        case '5': return QColor(0xAA, 0x00, 0xAA);
        case '6': return QColor(0xFF, 0xAA, 0x00);
        case '7': return QColor(0xAA, 0xAA, 0xAA);
        case '8': return QColor(0x55, 0x55, 0x55);
        case '9': return QColor(0x55, 0x55, 0xFF);
        case 'a': return QColor(0x55, 0xFF, 0x55);
        case 'b': return QColor(0x55, 0xFF, 0xFF);
        case 'c': return QColor(0xFF, 0x55, 0x55);
        case 'd': return QColor(0xFF, 0x55, 0xFF);
        case 'e': return QColor(0xFF, 0xFF, 0x55);
        case 'f': return QColor(0xFF, 0xFF, 0xFF);
    }
    return QColor();
}

static bool isCode(QChar c)
{
    const QChar l = c.toLower();
    return (l >= '0' && l <= '9') || (l >= 'a' && l <= 'f') || (l >= 'k' && l <= 'o') || l == 'r';
}

QList<QList<Span>> parse(const QString& text, QChar marker)
{
    const QColor white(0xFF, 0xFF, 0xFF);
    QList<QList<Span>> lines;
    lines.append({});
    Span current;
    current.color = white;

    auto flush = [&](Span& s) {
        if (!s.text.isEmpty()) {
            lines.last().append(s);
            s.text.clear();
        }
    };

    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text[i];
        if (c == '\n') {
            flush(current);
            lines.append({});
            continue;
        }
        if (c == marker && i + 1 < text.size() && isCode(text[i + 1])) {
            flush(current);
            const QChar code = text[++i].toLower();
            if (code == 'r') {
                current = Span();
                current.color = white;
            } else if (code == 'l') {
                current.bold = true;
            } else if (code == 'o') {
                current.italic = true;
            } else if (code == 'n') {
                current.underline = true;
            } else if (code == 'm') {
                current.strike = true;
            } else if (code == 'k') {
                current.obfuscated = true;
            } else {
                // a colour code also clears all formatting, like in the game
                const QColor color = colorForCode(code);
                current = Span();
                current.color = color;
            }
            continue;
        }
        current.text.append(c);
    }
    flush(current);
    return lines;
}

static QString convert(const QString& text, QChar from, QChar to)
{
    QString out;
    out.reserve(text.size());
    for (int i = 0; i < text.size(); ++i) {
        if (text[i] == from && i + 1 < text.size() && isCode(text[i + 1])) {
            out.append(to);
        } else {
            out.append(text[i]);
        }
    }
    return out;
}

QString ampersandToSection(const QString& text)
{
    return convert(text, '&', QChar(0x00A7));
}

QString sectionToAmpersand(const QString& text)
{
    return convert(text, QChar(0x00A7), '&');
}

}  // namespace Motd

MotdPreview::MotdPreview(QWidget* parent) : QWidget(parent)
{
    setMinimumHeight(96);
    m_obfuscationTimer.setInterval(120);
    connect(&m_obfuscationTimer, &QTimer::timeout, this, [this] {
        ++m_tick;
        update();
    });
}

void MotdPreview::setMotd(const QString& ampersandText)
{
    m_motd = ampersandText;
    // the scrambling animation only runs while there is something obfuscated to show
    const bool needsTimer = ampersandText.contains(QLatin1String("&k"), Qt::CaseInsensitive);
    if (needsTimer && !m_obfuscationTimer.isActive()) {
        m_obfuscationTimer.start();
    } else if (!needsTimer) {
        m_obfuscationTimer.stop();
    }
    update();
}

void MotdPreview::setServerName(const QString& name)
{
    m_name = name;
    update();
}

void MotdPreview::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing, false);  // the game's font is not smoothed

    p.fillRect(rect(), QColor(0x2B, 0x20, 0x16));  // dark "dirt" backdrop of the server list
    const QRect card = rect().adjusted(6, 6, -6, -6);
    p.fillRect(card, QColor(0, 0, 0, 120));
    p.setPen(QColor(0x80, 0x80, 0x80));
    p.drawRect(card.adjusted(0, 0, -1, -1));

    // placeholder for the server icon (64x64)
    const QRect icon(card.left() + 8, card.top() + 8, 64, 64);
    p.fillRect(icon, QColor(0x55, 0x80, 0x3A));
    p.setPen(QColor(0x3A, 0x5C, 0x28));
    p.drawRect(icon.adjusted(0, 0, -1, -1));

    QFont font(QStringLiteral("Monospace"));
    font.setStyleHint(QFont::Monospace);
    font.setPixelSize(16);
    font.setFixedPitch(true);

    auto drawSpan = [&](const Motd::Span& span, int& x, int baseline) {
        QFont f = font;
        f.setBold(span.bold);
        f.setItalic(span.italic);
        f.setUnderline(span.underline);
        f.setStrikeOut(span.strike);
        p.setFont(f);
        QString shown = span.text;
        if (span.obfuscated) {
            for (QChar& ch : shown) {
                if (!ch.isSpace()) {
                    ch = QChar(static_cast<ushort>('!' + QRandomGenerator::global()->bounded(60)));
                }
            }
        }
        const QColor shadow(span.color.red() / 4, span.color.green() / 4, span.color.blue() / 4);
        p.setPen(shadow);
        p.drawText(x + 1, baseline + 1, shown);
        p.setPen(span.color);
        p.drawText(x, baseline, shown);
        x += QFontMetrics(f).horizontalAdvance(shown);
    };

    const int textLeft = icon.right() + 10;
    // server name line
    {
        Motd::Span name;
        name.text = m_name;
        name.color = QColor(0xFF, 0xFF, 0xFF);
        int x = textLeft;
        drawSpan(name, x, card.top() + 24);
    }
    // two MOTD lines
    const auto lines = Motd::parse(Motd::ampersandToSection(m_motd), QChar(0x00A7));
    for (int row = 0; row < lines.size() && row < 2; ++row) {
        int x = textLeft;
        for (const auto& span : lines[row]) {
            drawSpan(span, x, card.top() + 44 + row * 18);
        }
    }

    // player counter on the right, like in the game
    p.setFont(font);
    const QString players = QStringLiteral("0/20");
    p.setPen(QColor(0x55, 0x55, 0x55));
    p.drawText(card.right() - QFontMetrics(font).horizontalAdvance(players) - 7, card.top() + 25, players);
    p.setPen(QColor(0xAA, 0xAA, 0xAA));
    p.drawText(card.right() - QFontMetrics(font).horizontalAdvance(players) - 8, card.top() + 24, players);
}
