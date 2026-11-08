// SPDX-License-Identifier: GPL-3.0-only
/*
 *  TaraLauncher - Minecraft Launcher
 *  "World" tab: open folder, backups (folder copies), restore, reset.
 */
#pragma once

#include <QWidget>

class QLabel;
class QListWidget;
class ServerProcess;

class WorldTab : public QWidget {
    Q_OBJECT
   public:
    explicit WorldTab(ServerProcess* server, QWidget* parent = nullptr);

   private:
    QString worldName() const;
    QString worldPath() const;
    QString backupsPath() const;
    bool ensureStopped();
    void reloadBackups();
    void createBackup();
    void restoreBackup();
    void deleteBackup();
    void resetWorld();

    ServerProcess* m_server = nullptr;
    QListWidget* m_backups = nullptr;
    QLabel* m_status = nullptr;
};
