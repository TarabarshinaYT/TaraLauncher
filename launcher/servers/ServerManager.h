// SPDX-License-Identifier: GPL-3.0-only
/*
 *  TaraLauncher - Minecraft Launcher
 *  Server component: keeps all configured servers, lets several run at once
 */
#pragma once

#include <QDir>
#include <QObject>
#include <QSet>
#include <memory>
#include <vector>

#include "ServerConfig.h"
#include "ServerProcess.h"

class ServerManager : public QObject {
    Q_OBJECT
   public:
    /** `rootDir` is the folder that contains one sub-folder per server. */
    explicit ServerManager(const QString& rootDir, QObject* parent = nullptr);

    QString rootDir() const { return m_rootDir; }
    /** Re-reads the root folder. Running servers are kept as they are. */
    void reload();

    QList<ServerProcess*> servers() const;
    ServerProcess* find(const QString& id) const;

    /**
     * Creates the folder and config for a new server. A free, not yet used port is
     * assigned automatically when config.port is not free. Also writes eula.txt and
     * server.properties (server-port) so the server is ready to start.
     * Returns nullptr and fills `error` on failure.
     */
    ServerProcess* create(ServerConfig config, QString* error = nullptr);
    /** Removes the server folder with the world. Refuses to delete a running server. */
    bool remove(const QString& id, QString* error = nullptr);

    /** Writes a changed config (name, memory, port...) to disk and updates the live object. */
    bool saveConfig(const QString& id, const ServerConfig& config, QString* error = nullptr);

    int runningCount() const;
    /** Sum of -Xmx of all active servers, to warn when several are started at once. */
    int activeMemoryMb() const;
    void stopAll();

    /** Ports used by configured servers, running or not. */
    QSet<int> reservedPorts(const QString& exceptId = QString()) const;

   signals:
    void serverAdded(ServerProcess* server);
    void serverRemoved(const QString& id);
    void serverStatusChanged(ServerProcess* server, ServerProcess::Status status);
    void playerJoined(ServerProcess* server, const QString& name);
    void playerLeft(ServerProcess* server, const QString& name);
    void serverCrashed(ServerProcess* server);
    void serverErrorLogged(ServerProcess* server, const QString& line);

   private:
    void attach(ServerProcess* server);
    static QString makeId(const QString& name, const QDir& root);

    QString m_rootDir;
    std::vector<ServerProcess*> m_servers;
};
