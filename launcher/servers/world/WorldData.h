// SPDX-License-Identifier: GPL-3.0-only
/*
 *  TaraLauncher - Minecraft Launcher
 *  Which chunks of a world exist, read from the region (.mca) file headers, plus player positions.
 *  Only 8 KB per region file is read, so even huge worlds load quickly.
 */
#pragma once

#include <QHash>
#include <QPointF>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QVector>

class WorldData {
   public:
    enum class Dimension { Overworld, Nether, End };

    struct PlayerMarker {
        QString name;
        QString uuid;
        QPointF block;  // x, z in blocks
        bool online = false;
        Dimension dimension = Dimension::Overworld;
    };

    /** Directories (relative to the server folder) that may hold the "region" folder for a dimension. */
    static QStringList regionDirCandidates(const QString& serverDir, const QString& levelName, Dimension dim);

    /** Reads all region headers from the first existing candidate directory. Returns the number of chunks found. */
    int loadRegions(const QStringList& candidates);

    bool hasChunk(int cx, int cz) const { return chunkTime(cx, cz) != 0; }
    /** Last-modified time (Unix seconds) of the chunk, 0 if it does not exist. */
    quint32 chunkTime(int cx, int cz) const;
    int chunkCount() const { return m_count; }
    QRect chunkBounds() const { return m_bounds; }  // in chunks, valid if chunkCount() > 0
    quint32 oldestTime() const { return m_oldest; }
    quint32 newestTime() const { return m_newest; }

    /** Player positions from <level>/playerdata/<uuid>.dat; names come from usercache.json. */
    static QList<PlayerMarker> readPlayers(const QString& serverDir, const QString& levelName, Dimension dim);
    /** World spawn from level.dat. Returns false if it cannot be read. */
    static bool readSpawn(const QString& serverDir, const QString& levelName, QPointF* blockXZ);

   private:
    static quint64 key(int rx, int rz) { return (static_cast<quint64>(static_cast<quint32>(rx)) << 32) | static_cast<quint32>(rz); }
    QHash<quint64, QVector<quint32>> m_regions;
    int m_count = 0;
    QRect m_bounds;
    quint32 m_oldest = 0;
    quint32 m_newest = 0;
};
