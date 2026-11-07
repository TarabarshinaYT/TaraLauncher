// SPDX-License-Identifier: GPL-3.0-only
/*
 *  TaraLauncher - Minecraft Launcher
 *  Server component: one running (or stopped) Minecraft server process
 */
#pragma once

#include <QObject>
#include <QPointer>
#include <QTimer>

#include "LoggedProcess.h"
#include "ServerConfig.h"

class ServerProcess : public QObject {
    Q_OBJECT
   public:
    enum class Status { Stopped, Starting, Running, Stopping, Crashed };
    Q_ENUM(Status)

    ServerProcess(const ServerConfig& config, const QString& directory, QObject* parent = nullptr);
    ~ServerProcess() override;

    const ServerConfig& config() const { return m_config; }
    void setConfig(const ServerConfig& config) { m_config = config; }
    QString directory() const { return m_directory; }
    Status status() const { return m_status; }
    bool isActive() const { return m_status == Status::Starting || m_status == Status::Running || m_status == Status::Stopping; }

    /** Starts the server. Returns false (and emits `failed`) if it cannot be started. */
    bool start();
    /** Graceful stop: sends "stop" to the console, force-kills after the timeout. */
    void stop();
    void kill();
    /** Sends a console command (without trailing newline). */
    void sendCommand(const QString& command);

    /** Full console log since the last start. */
    const QStringList& logLines() const { return m_log; }

   signals:
    void statusChanged(ServerProcess::Status status);
    void logAppended(const QStringList& lines);
    void failed(const QString& reason);

   private slots:
    void onLog(QStringList lines, MessageLevel level);
    void onProcessState(LoggedProcess::State state);
    void onStopTimeout();

   private:
    void setStatus(Status status);
    QString resolveJava() const;
    QStringList buildArguments() const;

    ServerConfig m_config;
    QString m_directory;
    Status m_status = Status::Stopped;
    QPointer<LoggedProcess> m_process;
    QTimer m_stopTimer;
    QStringList m_log;
};
