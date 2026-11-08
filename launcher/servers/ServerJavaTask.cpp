// SPDX-License-Identifier: GPL-3.0-only
#include "ServerJavaTask.h"

#include <QDir>
#include <algorithm>

#include "Application.h"
#include "FileSystem.h"
#include "SysInfo.h"
#include "java/JavaInstall.h"
#include "java/JavaInstallList.h"
#include "java/download/ArchiveDownloadTask.h"
#include "java/download/ManifestDownloadTask.h"
#include "java/download/SymlinkTask.h"
#include "meta/Index.h"
#include "meta/Version.h"
#include "meta/VersionList.h"
#include "minecraft/VersionFile.h"
#include "tasks/SequentialTask.h"

ServerJavaTask::ServerJavaTask(int requiredMajor, QObject* parent) : Task(parent), m_major(requiredMajor) {}

QString ServerJavaTask::findInstalled() const
{
    auto* list = APPLICATION->javalist();
    QString best;
    int bestMajor = 0;
    for (int i = 0; i < list->count(); ++i) {
        const auto java = std::dynamic_pointer_cast<JavaInstall>(list->at(i));
        if (!java || java->path.isEmpty()) {
            continue;
        }
        const int major = java->id.major();
        // Java 8 servers (1.16.5 and older, old Forge) really need 8; newer ones accept the same or a newer Java.
        const bool fits = (m_major <= 8) ? (major == 8) : (major >= m_major);
        if (!fits) {
            continue;
        }
        if (best.isEmpty() || major < bestMajor) {
            best = java->path;
            bestMajor = major;
        }
    }
    return best;
}

void ServerJavaTask::runStep(Task::Ptr task, std::function<void()> onSuccess)
{
    if (!task) {
        onSuccess();
        return;
    }
    m_steps << task;
    connect(task.get(), &Task::succeeded, this, [onSuccess] { onSuccess(); });
    connect(task.get(), &Task::failed, this, [this](QString reason) { emitFailed(reason); });
    if (!task->isRunning()) {
        task->start();
    }
}

void ServerJavaTask::executeTask()
{
    setStatus(tr("Поиск Java %1...").arg(m_major));
    auto* list = APPLICATION->javalist();
    if (list->isLoaded()) {
        afterInstalledListLoaded();
    } else {
        runStep(list->getLoadTask(), [this] { afterInstalledListLoaded(); });
    }
}

void ServerJavaTask::afterInstalledListLoaded()
{
    m_javaPath = findInstalled();
    if (!m_javaPath.isEmpty()) {
        emitSucceeded();
        return;
    }
    if (m_downloaded) {
        emitFailed(tr("Java %1 скачана, но лаунчер её не обнаружил.").arg(m_major));
        return;
    }
    loadMetadata();
}

void ServerJavaTask::loadMetadata()
{
    setStatus(tr("Поиск Java %1 для скачивания...").arg(m_major));
    const auto list = APPLICATION->metadataIndex()->get("net.adoptium.java");
    if (!list) {
        emitFailed(tr("Список Java недоступен."));
        return;
    }
    auto next = [this, list] {
        const auto version = list->getVersion(QStringLiteral("java%1").arg(m_major));
        if (!version) {
            emitFailed(tr("Для Java %1 нет доступных сборок.").arg(m_major));
            return;
        }
        runStep(version->loadTask(Net::Mode::Online, false), [this, version] {
            if (!version->data()) {
                emitFailed(tr("Не удалось прочитать данные о Java %1.").arg(m_major));
                return;
            }
            const QString os = SysInfo::getSupportedJavaArchitecture();
            QList<Java::MetadataPtr> candidates;
            for (const auto& rt : version->data()->runtimes) {
                if (rt->runtimeOS == os) {
                    candidates << rt;
                }
            }
            if (candidates.isEmpty()) {
                emitFailed(tr("Для вашей системы нет Java %1.").arg(m_major));
                return;
            }
            // newest build of that major version
            auto best = *std::max_element(candidates.begin(), candidates.end(),
                                          [](const Java::MetadataPtr& a, const Java::MetadataPtr& b) { return *a < *b; });
            downloadRuntime(best);
        });
    };
    if (list->isLoaded()) {
        next();
    } else {
        runStep(list->getLoadTask(), next);
    }
}

void ServerJavaTask::downloadRuntime(const Java::MetadataPtr& meta)
{
    setStatus(tr("Скачивание Java %1...").arg(m_major));
    const QString finalPath = FS::PathCombine(APPLICATION->javaPath(), meta->m_name);

    Task::Ptr task;
    switch (meta->downloadType) {
        case Java::DownloadType::Manifest:
            task = makeShared<Java::ManifestDownloadTask>(meta->url, finalPath, meta->checksumType, meta->checksumHash);
            break;
        case Java::DownloadType::Archive:
            task = makeShared<Java::ArchiveDownloadTask>(meta->url, finalPath, meta->checksumType, meta->checksumHash);
            break;
        case Java::DownloadType::Unknown:
            emitFailed(tr("Неизвестный тип загрузки Java."));
            return;
    }
#if defined(Q_OS_MACOS)
    auto seq = makeShared<SequentialTask>(tr("Установка Java"));
    seq->addTask(task);
    seq->addTask(makeShared<Java::SymlinkTask>(finalPath));
    task = seq;
#endif
    connect(task.get(), &Task::progress, this, &Task::setProgress);
    connect(task.get(), &Task::failed, this, [finalPath](const QString&) { FS::deletePath(finalPath); });

    runStep(task, [this] {
        // rescan installed Javas so the new one is found
        m_downloaded = true;
        setStatus(tr("Проверка установленной Java..."));
        runStep(APPLICATION->javalist()->getLoadTask(true), [this] { afterInstalledListLoaded(); });
    });
}
