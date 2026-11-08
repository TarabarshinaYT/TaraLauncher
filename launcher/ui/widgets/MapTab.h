// SPDX-License-Identifier: GPL-3.0-only
/*
 *  TaraLauncher - Minecraft Launcher
 *  "Map" tab: explored chunks of the world, spawn and player positions.
 */
#pragma once

#include <QTimer>
#include <QWidget>

#include "servers/world/WorldData.h"

class QComboBox;
class QLabel;
class ServerProcess;
class WorldMapView;

class MapTab : public QWidget {
    Q_OBJECT
   public:
    explicit MapTab(ServerProcess* server, QWidget* parent = nullptr);

   public slots:
    void reload(bool refit = false);

   private:
    void refreshLivePlayers();
    QString levelName() const;
    WorldData::Dimension dimension() const;

    ServerProcess* m_server = nullptr;
    WorldData m_data;
    WorldMapView* m_view = nullptr;
    QComboBox* m_dimension = nullptr;
    QLabel* m_status = nullptr;
    QLabel* m_hover = nullptr;
    QTimer m_timer;
    QList<WorldData::PlayerMarker> m_filePlayers;
    bool m_firstLoad = true;
};
