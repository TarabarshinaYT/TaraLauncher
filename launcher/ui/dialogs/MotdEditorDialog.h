// SPDX-License-Identifier: GPL-3.0-only
/*
 *  TaraLauncher - Minecraft Launcher
 *  Visual editor for the server description (MOTD) with a Minecraft-style preview.
 */
#pragma once

#include <QDialog>

class MotdPreview;
class QPlainTextEdit;

class MotdEditorDialog : public QDialog {
    Q_OBJECT
   public:
    /** `storedMotd` uses the real section sign and real line breaks (already unescaped). */
    explicit MotdEditorDialog(const QString& storedMotd, const QString& serverName, QWidget* parent = nullptr);

    /** Result in stored form: section signs and "\n" for the line break, at most two lines. */
    QString motd() const;

   private:
    void insertCode(const QString& code);
    void limitLines();

    QPlainTextEdit* m_edit = nullptr;
    MotdPreview* m_preview = nullptr;
};
