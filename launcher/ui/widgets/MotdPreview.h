// SPDX-License-Identifier: GPL-3.0-only
/*
 *  TaraLauncher - Minecraft Launcher
 *  Draws a server MOTD the way the Minecraft server list does.
 */
#pragma once

#include <QColor>
#include <QList>
#include <QTimer>
#include <QWidget>

namespace Motd {
struct Span {
    QString text;
    QColor color;
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strike = false;
    bool obfuscated = false;
};
/** Parses text with colour codes. `marker` is '§' (stored form) or '&' (typing form). */
QList<QList<Span>> parse(const QString& text, QChar marker);
/** Converts typing form ("&a") to stored form ("§a"). Only real codes are converted. */
QString ampersandToSection(const QString& text);
QString sectionToAmpersand(const QString& text);
QColor colorForCode(QChar code);
}  // namespace Motd

class MotdPreview : public QWidget {
    Q_OBJECT
   public:
    explicit MotdPreview(QWidget* parent = nullptr);

    /** Text in typing form (with & codes). Up to two lines are shown. */
    void setMotd(const QString& ampersandText);
    void setServerName(const QString& name);

    QSize sizeHint() const override { return QSize(560, 96); }
    QSize minimumSizeHint() const override { return QSize(420, 96); }

   protected:
    void paintEvent(QPaintEvent* event) override;

   private:
    QString m_motd;
    QString m_name = QStringLiteral("Minecraft Server");
    QTimer m_obfuscationTimer;
    int m_tick = 0;
};
