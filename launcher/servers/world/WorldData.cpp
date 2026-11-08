// SPDX-License-Identifier: GPL-3.0-only
#include "WorldData.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <cmath>

#include "Nbt.h"

QStringList WorldData::regionDirCandidates(const QString& serverDir, const QString& levelName, Dimension dim)
{
    const QDir root(serverDir);
    const QString level = levelName.isEmpty() ? QStringLiteral("world") : levelName;
    QStringList out;
    switch (dim) {
        case Dimension::Overworld:
            out << root.filePath(level + "/region")                                // classic
                << root.filePath(level + "/dimensions/minecraft/overworld/region");  // newer layout
            break;
        case Dimension::Nether:
            out << root.filePath(level + "/DIM-1/region")
                << root.filePath(level + "_nether/DIM-1/region")  // Paper and friends
                << root.filePath(level + "/dimensions/minecraft/the_nether/region");
            break;
        case Dimension::End:
            out << root.filePath(level + "/DIM1/region")
                << root.filePath(level + "_the_end/DIM1/region")
                << root.filePath(level + "/dimensions/minecraft/the_end/region");
            break;
    }
    return out;
}

int WorldData::loadRegions(const QStringList& candidates)
{
    m_regions.clear();
    m_count = 0;
    m_bounds = QRect();
    m_oldest = m_newest = 0;

    static const QRegularExpression name(QStringLiteral("^r\\.(-?\\d+)\\.(-?\\d+)\\.mca$"));
    for (const QString& dirPath : candidates) {
        const QDir dir(dirPath);
        if (!dir.exists()) {
            continue;
        }
        for (const QFileInfo& info : dir.entryInfoList({ QStringLiteral("r.*.*.mca") }, QDir::Files)) {
            const auto m = name.match(info.fileName());
            if (!m.hasMatch()) {
                continue;
            }
            const int rx = m.captured(1).toInt();
            const int rz = m.captured(2).toInt();
            QFile file(info.absoluteFilePath());
            if (!file.open(QIODevice::ReadOnly)) {
                continue;
            }
            const QByteArray header = file.read(8192);  // 4096 bytes of locations, 4096 bytes of timestamps
            if (header.size() < 8192) {
                continue;  // empty or truncated file
            }
            QVector<quint32> times(1024, 0);
            bool any = false;
            for (int i = 0; i < 1024; ++i) {
                const auto* loc = reinterpret_cast<const quint8*>(header.constData()) + i * 4;
                const quint32 offset = (loc[0] << 16) | (loc[1] << 8) | loc[2];
                if (offset == 0) {
                    continue;  // chunk not generated
                }
                const auto* ts = reinterpret_cast<const quint8*>(header.constData()) + 4096 + i * 4;
                quint32 t = (static_cast<quint32>(ts[0]) << 24) | (ts[1] << 16) | (ts[2] << 8) | ts[3];
                if (t == 0) {
                    t = 1;  // exists, but no time recorded
                }
                times[i] = t;
                any = true;
                ++m_count;
                if (m_oldest == 0 || t < m_oldest) {
                    m_oldest = t;
                }
                m_newest = qMax(m_newest, t);

                const int cx = rx * 32 + (i % 32);
                const int cz = rz * 32 + (i / 32);
                const QRect cell(cx, cz, 1, 1);
                m_bounds = m_bounds.isNull() ? cell : m_bounds.united(cell);
            }
            if (any) {
                m_regions.insert(key(rx, rz), times);
            }
        }
        if (!m_regions.isEmpty()) {
            break;  // use the first directory that actually contains a world
        }
    }
    return m_count;
}

quint32 WorldData::chunkTime(int cx, int cz) const
{
    // floor division so negative coordinates land in the right region
    const int rx = cx >> 5;
    const int rz = cz >> 5;
    const auto it = m_regions.constFind(key(rx, rz));
    if (it == m_regions.constEnd()) {
        return 0;
    }
    return (*it)[(cx & 31) + (cz & 31) * 32];
}

namespace {
QHash<QString, QString> readUserCache(const QString& serverDir)
{
    QHash<QString, QString> names;  // uuid (lower case, with dashes) -> name
    QFile file(QDir(serverDir).filePath(QStringLiteral("usercache.json")));
    if (file.open(QIODevice::ReadOnly)) {
        for (const auto& v : QJsonDocument::fromJson(file.readAll()).array()) {
            const auto o = v.toObject();
            names.insert(o.value("uuid").toString().toLower(), o.value("name").toString());
        }
    }
    return names;
}

double listNumber(const QVariantList& list, int index)
{
    return index < list.size() ? list[index].toDouble() : 0.0;
}

WorldData::Dimension dimensionFrom(const QVariant& value)
{
    if (value.typeId() == QMetaType::QString) {
        const QString s = value.toString();
        if (s.contains(QLatin1String("nether"))) {
            return WorldData::Dimension::Nether;
        }
        if (s.contains(QLatin1String("end"))) {
            return WorldData::Dimension::End;
        }
        return WorldData::Dimension::Overworld;
    }
    switch (value.toLongLong()) {  // old numeric ids
        case -1: return WorldData::Dimension::Nether;
        case 1: return WorldData::Dimension::End;
        default: return WorldData::Dimension::Overworld;
    }
}
}  // namespace

QList<WorldData::PlayerMarker> WorldData::readPlayers(const QString& serverDir, const QString& levelName, Dimension dim)
{
    QList<PlayerMarker> out;
    const QString level = levelName.isEmpty() ? QStringLiteral("world") : levelName;
    const auto names = readUserCache(serverDir);

    const QStringList dirs = { QDir(serverDir).filePath(level + "/playerdata"), QDir(serverDir).filePath(level + "/players/data") };
    for (const QString& dirPath : dirs) {
        const QDir dir(dirPath);
        for (const QFileInfo& info : dir.entryInfoList({ QStringLiteral("*.dat") }, QDir::Files)) {
            const QVariant nbt = Nbt::readGzipFile(info.absoluteFilePath());
            if (!nbt.isValid()) {
                continue;
            }
            const QVariantMap map = nbt.toMap();
            const QVariantList pos = map.value("Pos").toList();
            if (pos.size() < 3) {
                continue;
            }
            if (dimensionFrom(map.value("Dimension")) != dim) {
                continue;
            }
            PlayerMarker marker;
            marker.uuid = info.completeBaseName().toLower();
            marker.name = names.value(marker.uuid, marker.uuid.left(8));
            marker.block = QPointF(listNumber(pos, 0), listNumber(pos, 2));
            marker.dimension = dim;
            out << marker;
        }
        if (!out.isEmpty()) {
            break;
        }
    }
    return out;
}

bool WorldData::readSpawn(const QString& serverDir, const QString& levelName, QPointF* blockXZ)
{
    const QString level = levelName.isEmpty() ? QStringLiteral("world") : levelName;
    const QVariant nbt = Nbt::readGzipFile(QDir(serverDir).filePath(level + "/level.dat"));
    if (!nbt.isValid()) {
        return false;
    }
    const QVariantMap data = nbt.toMap().value("Data").toMap();
    if (data.contains("SpawnX")) {
        *blockXZ = QPointF(data.value("SpawnX").toDouble(), data.value("SpawnZ").toDouble());
        return true;
    }
    // newer versions keep the spawn as a compound with an int array "pos"
    const QVariantList pos = data.value("spawn").toMap().value("pos").toList();
    if (pos.size() >= 3) {
        *blockXZ = QPointF(pos[0].toDouble(), pos[2].toDouble());
        return true;
    }
    return false;
}
