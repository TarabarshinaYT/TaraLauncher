// SPDX-License-Identifier: GPL-3.0-only
/*
 *  TaraLauncher - Minecraft Launcher
 *  Pannable, zoomable chunk map of a world with player markers.
 */
#pragma once

#include <QWidget>

#include "servers/world/WorldData.h"

class WorldMapView : public QWidget {
    Q_OBJECT
   public:
    explicit WorldMapView(QWidget* parent = nullptr);

    void setData(const WorldData* data) { m_data = data; update(); }
    void setPlayers(const QList<WorldData::PlayerMarker>& players);
    void setSpawn(const QPointF& block, bool valid);
    /** Centers the view on a block position. */
    void centerOnBlock(const QPointF& block);
    /** Zooms out so all known chunks are visible. */
    void fitWorld();

   signals:
    /** Block coordinates under the mouse, for the status line. */
    void hoverBlock(int x, int z, bool explored);
    void hoverLeft();

   protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void leaveEvent(QEvent* event) override;

   private:
    QPointF screenToChunk(const QPointF& p) const;
    QPointF chunkToScreen(const QPointF& chunk) const;

    const WorldData* m_data = nullptr;
    QList<WorldData::PlayerMarker> m_players;
    QPointF m_spawn;
    bool m_hasSpawn = false;
    QPointF m_center;   // chunk coordinates (fractional)
    double m_scale = 4; // screen pixels per chunk
    bool m_dragging = false;
    QPoint m_lastMouse;
};
