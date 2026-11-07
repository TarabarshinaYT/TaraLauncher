// SPDX-License-Identifier: GPL-3.0-only
#include "ServerInstaller.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "Application.h"
#include "net/Request.h"

ServerInstaller::ServerInstaller(const ServerConfig& config, const QString& directory, QObject* parent)
    : Task(parent), m_config(config), m_dir(directory)
{}

bool ServerInstaller::isSupported(ServerType type)
{
    return type == ServerType::Vanilla || type == ServerType::Paper || type == ServerType::Purpur || type == ServerType::Fabric;
}

void ServerInstaller::executeTask()
{
    setStatus(tr("Поиск файла сервера..."));
    switch (m_config.type) {
        case ServerType::Vanilla: resolveVanilla(); break;
        case ServerType::Fabric: resolveFabric(); break;
        case ServerType::Paper: resolvePaper(); break;
        case ServerType::Purpur: resolvePurpur(); break;
        default:
            emitFailed(tr("Тип сервера «%1» пока не поддерживается.").arg(serverTypeName(m_config.type)));
            break;
    }
}

void ServerInstaller::fetchAll(const QList<QUrl>& urls, std::function<void(const QList<QByteArray>&)> done)
{
    m_job.reset(new NetJob(tr("Информация о сервере"), APPLICATION->network()));
    QList<QByteArray*> bodies;
    for (const QUrl& url : urls) {
        auto [request, data] = Net::Request::makeByteArray(url);
        bodies << data;
        m_job->addNetAction(request);
    }
    connect(m_job.get(), &Task::failed, this, [this](QString reason) { emitFailed(tr("Не удалось получить данные: %1").arg(reason)); });
    connect(m_job.get(), &Task::succeeded, this, [bodies, done = std::move(done)]() {
        QList<QByteArray> out;
        for (QByteArray* b : bodies) {
            out << *b;
        }
        done(out);
    });
    m_job->start();
}

void ServerInstaller::download(const QUrl& url)
{
    setStatus(tr("Скачивание server.jar..."));
    m_job.reset(new NetJob(tr("Скачивание сервера"), APPLICATION->network()));
    m_job->addNetAction(Net::Request::makeFile(url, QDir(m_dir).filePath("server.jar")));
    connect(m_job.get(), &Task::progress, this, &Task::setProgress);
    connect(m_job.get(), &Task::failed, this, [this](QString reason) { emitFailed(tr("Не удалось скачать сервер: %1").arg(reason)); });
    connect(m_job.get(), &Task::succeeded, this, [this]() { emitSucceeded(); });
    m_job->start();
}

void ServerInstaller::resolveVanilla()
{
    fetchAll({ QUrl("https://piston-meta.mojang.com/mc/game/version_manifest_v2.json") }, [this](const QList<QByteArray>& bodies) {
        const auto versions = QJsonDocument::fromJson(bodies.value(0)).object().value("versions").toArray();
        QUrl versionJson;
        for (const auto& v : versions) {
            if (v.toObject().value("id").toString() == m_config.minecraftVersion) {
                versionJson = QUrl(v.toObject().value("url").toString());
                break;
            }
        }
        if (!versionJson.isValid()) {
            emitFailed(tr("Версия %1 не найдена в списке Mojang.").arg(m_config.minecraftVersion));
            return;
        }
        fetchAll({ versionJson }, [this](const QList<QByteArray>& bodies2) {
            const auto server = QJsonDocument::fromJson(bodies2.value(0)).object().value("downloads").toObject().value("server").toObject();
            const QUrl url(server.value("url").toString());
            if (!url.isValid() || url.isEmpty()) {
                emitFailed(tr("У версии %1 нет серверного файла.").arg(m_config.minecraftVersion));
                return;
            }
            download(url);
        });
    });
}

void ServerInstaller::resolveFabric()
{
    const QString mc = m_config.minecraftVersion;
    fetchAll({ QUrl("https://meta.fabricmc.net/v2/versions/loader/" + mc), QUrl("https://meta.fabricmc.net/v2/versions/installer") },
             [this, mc](const QList<QByteArray>& bodies) {
                 QString loader = m_config.loaderVersion;
                 if (loader.isEmpty()) {
                     for (const auto& e : QJsonDocument::fromJson(bodies.value(0)).array()) {
                         const auto l = e.toObject().value("loader").toObject();
                         if (l.value("stable").toBool()) {
                             loader = l.value("version").toString();
                             break;
                         }
                     }
                 }
                 QString installer;
                 for (const auto& e : QJsonDocument::fromJson(bodies.value(1)).array()) {
                     if (e.toObject().value("stable").toBool()) {
                         installer = e.toObject().value("version").toString();
                         break;
                     }
                 }
                 if (loader.isEmpty() || installer.isEmpty()) {
                     emitFailed(tr("Fabric не поддерживает версию %1.").arg(mc));
                     return;
                 }
                 m_config.loaderVersion = loader;
                 download(QUrl(QStringLiteral("https://meta.fabricmc.net/v2/versions/loader/%1/%2/%3/server/jar").arg(mc, loader, installer)));
             });
}

void ServerInstaller::resolvePaper()
{
    const QString mc = m_config.minecraftVersion;
    fetchAll({ QUrl("https://fill.papermc.io/v3/projects/paper/versions/" + mc + "/builds") }, [this, mc](const QList<QByteArray>& bodies) {
        for (const auto& e : QJsonDocument::fromJson(bodies.value(0)).array()) {
            const auto build = e.toObject();
            if (build.value("channel").toString() != "STABLE") {
                continue;
            }
            const QUrl url(build.value("downloads").toObject().value("server:default").toObject().value("url").toString());
            if (url.isValid() && !url.isEmpty()) {
                m_config.loaderVersion = QString::number(build.value("id").toInt());
                download(url);
                return;
            }
        }
        emitFailed(tr("Для Paper нет стабильной сборки под версию %1.").arg(mc));
    });
}

void ServerInstaller::resolvePurpur()
{
    // Purpur serves the latest build of a version from a fixed address.
    download(QUrl("https://api.purpurmc.org/v2/purpur/" + m_config.minecraftVersion + "/latest/download"));
}
