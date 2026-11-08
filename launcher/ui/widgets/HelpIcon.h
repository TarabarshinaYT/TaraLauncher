// SPDX-License-Identifier: GPL-3.0-only
/*
 *  TaraLauncher - Minecraft Launcher
 *  Small round "?" icon that shows an explanation popup when hovered.
 */
#pragma once

#include <QLabel>

class HelpIcon : public QLabel {
    Q_OBJECT
   public:
    explicit HelpIcon(const QString& helpText, QWidget* parent = nullptr);

    void setHelpText(const QString& text);

    /** Returns a widget holding `field` with a HelpIcon to its right. Ownership goes to the layout it is added to. */
    static QWidget* wrap(QWidget* field, const QString& helpText, QWidget* parent = nullptr);

   protected:
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;  // tap on touch screens

   private:
    void showPopup();
    QString m_html;
};
