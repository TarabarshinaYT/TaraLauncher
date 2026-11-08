// SPDX-License-Identifier: GPL-3.0-only
/*
 *  TaraLauncher - Minecraft Launcher
 *  Finds an installed Java of the right major version for a server, or downloads one
 *  through the launcher's own Java metadata (Adoptium), then reports its path.
 */
#pragma once

#include <functional>

#include "java/JavaMetadata.h"
#include "tasks/Task.h"

class ServerJavaTask : public Task {
    Q_OBJECT
   public:
    explicit ServerJavaTask(int requiredMajor, QObject* parent = nullptr);

    /** Path of the java executable. Valid after the task succeeded. */
    QString javaPath() const { return m_javaPath; }

   protected:
    void executeTask() override;

   private:
    QString findInstalled() const;
    void runStep(Task::Ptr task, std::function<void()> onSuccess);
    void afterInstalledListLoaded();
    void loadMetadata();
    void downloadRuntime(const Java::MetadataPtr& runtime);

    int m_major = 21;
    QString m_javaPath;
    QList<Task::Ptr> m_steps;  // keeps steps alive; a step may still be emitting its own signal
    bool m_downloaded = false;
};
