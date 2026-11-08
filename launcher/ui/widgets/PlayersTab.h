// SPDX-License-Identifier: GPL-3.0-only
/*
 *  TaraLauncher - Minecraft Launcher
 *  "Players" tab: who is online, who played when and for how long.
 */
#pragma once

#include <QTimer>
#include <QWidget>

class QTableWidget;
class ServerProcess;

class PlayersTab : public QWidget {
    Q_OBJECT
   public:
    explicit PlayersTab(ServerProcess* server, QWidget* parent = nullptr);

   public slots:
    void refresh();

   private:
    ServerProcess* m_server = nullptr;
    QTableWidget* m_players = nullptr;
    QTableWidget* m_sessions = nullptr;
    QTimer m_timer;
};
