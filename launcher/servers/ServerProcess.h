// SPDX-License-Identifier: GPL-3.0-only
/*
 *  TaraLauncher - Minecraft Launcher
 *  Server component: one Minecraft server that runs independently of the launcher.
 *
 *  The server is started as a detached process, so closing the launcher never stops it.
 *  The console is read from logs/latest.log, commands and "stop" go through RCON, and the
 *  process id is stored in tara-server.pid so a restarted launcher finds the server again.
 */
#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

#include "PlayerHistory.h"
#include "ServerConfig.h"

class ServerProcess : public QObject {
    Q_OBJECT
   public:
    enum class Status { Stopped, Starting, Running, Stopping, Crashed };
    Q_ENUM(Status)

    ServerProcess(const ServerConfig& config, const QString& directory, QObject* parent = nullptr);
    ~ServerProcess() override = default;  // deliberately does NOT stop the server

    const ServerConfig& config() const { return m_config; }
    void setConfig(const ServerConfig& config) { m_config = config; }
    QString directory() const { return m_directory; }
    Status status() const { return m_status; }
    bool isActive() const { return m_status == Status::Starting || m_status == Status::Running || m_status == Status::Stopping; }

    /** Starts the server. Returns false (and emits `failed`) if it cannot be started. */
    bool start();
    /** Graceful stop through RCON ("stop"); force-kills after 30 seconds if the server does not exit. */
    void stop();
    /** Immediately kills the server process. May lose the last minutes of the world. */
    void kill();
    /** Runs a console command through RCON. */
    void sendCommand(const QString& command);
    /** Runs a command through RCON and returns the answer without writing anything to the console. */
    bool query(const QString& command, QString* reply);

    /** Console text of the current run (or the tail of the log after reattaching). */
    const QStringList& logLines() const { return m_log; }

    PlayerHistory& history() { return m_history; }
    const PlayerHistory& history() const { return m_history; }

   signals:
    void statusChanged(ServerProcess::Status status);
    void logAppended(const QStringList& lines);
    void failed(const QString& reason);
    void playerJoined(const QString& name);
    void playerLeft(const QString& name);
    /** An ERROR line appeared in the server log (rate limited). */
    void errorLogged(const QString& line);
    /** The server process died without being asked to stop. */
    void crashed();

   private slots:
    void poll();

   private:
    void setStatus(Status status);
    void appendLines(const QStringList& lines);
    void readNewLog();
    bool ensureRcon(QString* error);
    QString resolveJava() const;
    QStringList buildArguments() const;
    QString pidFilePath() const;
    QString logFilePath() const;
    void writePidFile() const;
    void parseEvents(const QStringList& lines);
    void reconcileOnlinePlayers();

    ServerConfig m_config;
    QString m_directory;
    Status m_status = Status::Stopped;
    qint64 m_pid = 0;
    qint64 m_logOffset = 0;
    bool m_stopRequested = false;
    QElapsedTimer m_stopClock;
    QTimer m_poll;
    QStringList m_log;
    PlayerHistory m_history;
    QElapsedTimer m_errorClock;
    bool m_needsReconcile = false;
};
