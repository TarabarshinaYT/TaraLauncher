// SPDX-License-Identifier: GPL-3.0-only
#include "PlayerHistory.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <algorithm>

namespace {
constexpr int kMaxSessions = 1000;
QString iso(const QDateTime& t)
{
    return t.isValid() ? t.toString(Qt::ISODate) : QString();
}
QDateTime fromIso(const QString& s)
{
    return QDateTime::fromString(s, Qt::ISODate);
}
}  // namespace

PlayerHistory::PlayerHistory(const QString& directory) : m_dir(directory)
{
    if (!m_dir.isEmpty()) {
        load();
    }
}

void PlayerHistory::setDirectory(const QString& directory)
{
    m_dir = directory;
}

QString PlayerHistory::path() const
{
    return QDir(m_dir).filePath(QStringLiteral("tara-players.json"));
}

bool PlayerHistory::load()
{
    m_players.clear();
    m_sessions.clear();
    QFile file(path());
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return false;
    }
    const QJsonObject root = doc.object();
    for (const auto& v : root.value("players").toArray()) {
        const QJsonObject o = v.toObject();
        Player p;
        p.name = o.value("name").toString();
        if (p.name.isEmpty()) {
            continue;
        }
        p.firstSeen = fromIso(o.value("firstSeen").toString());
        p.lastSeen = fromIso(o.value("lastSeen").toString());
        p.totalSeconds = static_cast<qint64>(o.value("totalSeconds").toDouble());
        p.sessions = o.value("sessions").toInt();
        p.currentJoin = fromIso(o.value("currentJoin").toString());
        p.online = p.currentJoin.isValid();
        m_players.insert(p.name, p);
    }
    for (const auto& v : root.value("log").toArray()) {
        const QJsonObject o = v.toObject();
        Session s;
        s.name = o.value("name").toString();
        s.join = fromIso(o.value("join").toString());
        s.leave = fromIso(o.value("leave").toString());
        if (!s.name.isEmpty() && s.join.isValid()) {
            m_sessions.append(s);
        }
    }
    return true;
}

bool PlayerHistory::save() const
{
    if (m_dir.isEmpty()) {
        return false;
    }
    QJsonArray players;
    for (const Player& p : m_players) {
        QJsonObject o;
        o["name"] = p.name;
        o["firstSeen"] = iso(p.firstSeen);
        o["lastSeen"] = iso(p.lastSeen);
        o["totalSeconds"] = static_cast<double>(p.totalSeconds);
        o["sessions"] = p.sessions;
        o["currentJoin"] = p.online ? iso(p.currentJoin) : QString();
        players.append(o);
    }
    QJsonArray log;
    for (const Session& s : m_sessions) {
        QJsonObject o;
        o["name"] = s.name;
        o["join"] = iso(s.join);
        o["leave"] = iso(s.leave);
        log.append(o);
    }
    QJsonObject root;
    root["players"] = players;
    root["log"] = log;

    QSaveFile file(path());
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    return file.commit();
}

void PlayerHistory::playerJoined(const QString& name, const QDateTime& at)
{
    Player& p = m_players[name];
    p.name = name;
    if (!p.firstSeen.isValid()) {
        p.firstSeen = at;
    }
    if (p.online) {
        // a join without a leave: close the previous session at the new join time
        playerLeft(name, at);
    }
    p.online = true;
    p.currentJoin = at;
    p.lastSeen = at;
    ++p.sessions;
    save();
}

void PlayerHistory::playerLeft(const QString& name, const QDateTime& at)
{
    auto it = m_players.find(name);
    if (it == m_players.end() || !it->online) {
        return;
    }
    Player& p = *it;
    const qint64 seconds = qMax<qint64>(0, p.currentJoin.secsTo(at));
    p.totalSeconds += seconds;
    p.lastSeen = at;
    m_sessions.append({ name, p.currentJoin, at });
    while (m_sessions.size() > kMaxSessions) {
        m_sessions.removeFirst();
    }
    p.online = false;
    p.currentJoin = QDateTime();
    save();
}

void PlayerHistory::closeAll(const QDateTime& at)
{
    const QStringList names = onlineNames();
    for (const QString& name : names) {
        playerLeft(name, at);
    }
}

void PlayerHistory::keepOnly(const QStringList& onlineNamesNow, const QDateTime& at)
{
    for (const QString& name : onlineNames()) {
        if (!onlineNamesNow.contains(name, Qt::CaseSensitive)) {
            playerLeft(name, at);
        }
    }
}

QStringList PlayerHistory::onlineNames() const
{
    QStringList out;
    for (const Player& p : m_players) {
        if (p.online) {
            out << p.name;
        }
    }
    return out;
}

QList<PlayerHistory::Player> PlayerHistory::players() const
{
    QList<Player> out = m_players.values();
    std::sort(out.begin(), out.end(), [](const Player& a, const Player& b) {
        if (a.online != b.online) {
            return a.online;
        }
        return a.lastSeen > b.lastSeen;
    });
    return out;
}

QList<PlayerHistory::Session> PlayerHistory::recentSessions(int limit) const
{
    QList<Session> out = m_sessions;
    std::reverse(out.begin(), out.end());
    if (out.size() > limit) {
        out = out.mid(0, limit);
    }
    return out;
}

QString PlayerHistory::formatDuration(qint64 seconds)
{
    const qint64 h = seconds / 3600;
    const qint64 m = (seconds % 3600) / 60;
    const qint64 s = seconds % 60;
    if (h > 0) {
        return QObject::tr("%1 ч %2 мин").arg(h).arg(m);
    }
    if (m > 0) {
        return QObject::tr("%1 мин %2 с").arg(m).arg(s);
    }
    return QObject::tr("%1 с").arg(s);
}
