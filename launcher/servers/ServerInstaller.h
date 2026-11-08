// SPDX-License-Identifier: GPL-3.0-only
/*
 *  TaraLauncher - Minecraft Launcher
 *  Server component: downloads server.jar for a freshly created server
 */
#pragma once

#include <QString>
#include <functional>

#include "net/NetJob.h"
#include "tasks/Task.h"
#include "ServerConfig.h"

/**
 * Resolves the download URL for the server type and saves it as <dir>/server.jar.
 * Supported now: Vanilla, Paper, Purpur, Fabric. Quilt, Forge and NeoForge need an
 * installer step and are reported as unsupported.
 */
class ServerInstaller : public Task {
    Q_OBJECT
   public:
    ServerInstaller(const ServerConfig& config, const QString& directory, QObject* parent = nullptr);

    static bool isSupported(ServerType type);
    /** Config with the data learned while installing (loader build, required Java). */
    const ServerConfig& resultConfig() const { return m_config; }

   protected:
    void executeTask() override;

   private:
    /** Downloads `urls` into memory, then calls `done` with the bodies in the same order. */
    void fetchAll(const QList<QUrl>& urls, std::function<void(const QList<QByteArray>&)> done);
    void download(const QUrl& url);

    void resolveVanilla();
    void resolveFabric();
    void resolvePaper();
    void resolvePurpur();

    ServerConfig m_config;
    QString m_dir;
    NetJob::Ptr m_job;
};
