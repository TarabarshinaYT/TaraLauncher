// SPDX-License-Identifier: GPL-3.0-only
/*
 *  TaraLauncher - Minecraft Launcher
 *  Who joined a server, when and for how long. Stored in tara-players.json inside the server folder.
 */
#pragma once

#include <QDateTime>
#include <QObject>
#include <QStringList>
#include <QList>
#include <QMap>
#include <QString>

class PlayerHistory {
   public:
    struct Player {
        QString name;
        QDateTime firstSeen;
        QDateTime lastSeen;
        qint64 totalSeconds = 0;
        int sessions = 0;
        bool online = false;
        QDateTime currentJoin;  // valid while online
    };
    struct Session {
        QString name;
        QDateTime join;
        QDateTime leave;
    };

    explicit PlayerHistory(const QString& directory = QString());

    void setDirectory(const QString& directory);
    bool load();
    bool save() const;

    void playerJoined(const QString& name, const QDateTime& at);
    void playerLeft(const QString& name, const QDateTime& at);
    /** Ends all open sessions (the server stopped or crashed). */
    void closeAll(const QDateTime& at);
    /** Ends sessions of everyone who is not in `onlineNames` (used after the launcher was restarted). */
    void keepOnly(const QStringList& onlineNames, const QDateTime& at);

    QList<Player> players() const;  // most recently seen first
    QList<Session> recentSessions(int limit = 200) const;
    QStringList onlineNames() const;

    static QString formatDuration(qint64 seconds);

   private:
    QString path() const;

    QString m_dir;
    QMap<QString, Player> m_players;
    QList<Session> m_sessions;  // newest last
};
