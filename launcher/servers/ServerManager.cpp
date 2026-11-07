// SPDX-License-Identifier: GPL-3.0-only
#include "ServerManager.h"

#include <QDir>
#include <algorithm>
#include <QFile>
#include <QRegularExpression>

#include "PortAllocator.h"
#include "ServerProperties.h"

ServerManager::ServerManager(const QString& rootDir, QObject* parent) : QObject(parent), m_rootDir(rootDir)
{
    QDir().mkpath(m_rootDir);
    reload();
}

void ServerManager::reload()
{
    const QDir root(m_rootDir);
    for (const QString& name : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        if (find(name)) {
            continue;
        }
        const QString dir = root.filePath(name);
        ServerConfig config;
        if (!config.load(dir)) {
            continue;  // not a TaraLauncher server folder
        }
        attach(new ServerProcess(config, dir, this));
    }
}

void ServerManager::attach(ServerProcess* server)
{
    m_servers.push_back(server);
    connect(server, &ServerProcess::statusChanged, this,
            [this, server](ServerProcess::Status status) { emit serverStatusChanged(server, status); });
    emit serverAdded(server);
}

QList<ServerProcess*> ServerManager::servers() const
{
    return QList<ServerProcess*>(m_servers.begin(), m_servers.end());
}

ServerProcess* ServerManager::find(const QString& id) const
{
    for (ServerProcess* s : m_servers) {
        if (s->config().id == id) {
            return s;
        }
    }
    return nullptr;
}

QSet<int> ServerManager::reservedPorts(const QString& exceptId) const
{
    QSet<int> ports;
    for (ServerProcess* s : m_servers) {
        if (s->config().id != exceptId) {
            ports.insert(s->config().port);
        }
    }
    return ports;
}

QString ServerManager::makeId(const QString& name, const QDir& root)
{
    static const QRegularExpression bad(QStringLiteral("[^A-Za-z0-9._-]+"));
    QString base = name.trimmed();
    base.replace(bad, QStringLiteral("-"));
    base = base.left(40);
    while (base.startsWith('-') || base.startsWith('.')) {
        base.remove(0, 1);
    }
    if (base.isEmpty()) {
        base = QStringLiteral("server");
    }
    QString id = base;
    for (int i = 2; root.exists(id); ++i) {
        id = QStringLiteral("%1-%2").arg(base).arg(i);
    }
    return id;
}

ServerProcess* ServerManager::create(ServerConfig config, QString* error)
{
    auto fail = [&](const QString& text) -> ServerProcess* {
        if (error) {
            *error = text;
        }
        return nullptr;
    };

    if (config.minecraftVersion.isEmpty()) {
        return fail(tr("Не выбрана версия Minecraft."));
    }
    const QDir root(m_rootDir);
    config.id = makeId(config.name.isEmpty() ? config.minecraftVersion : config.name, root);
    if (config.name.isEmpty()) {
        config.name = config.minecraftVersion;
    }

    const QSet<int> reserved = reservedPorts();
    if (reserved.contains(config.port) || !ServerPorts::isFree(config.port)) {
        const int port = ServerPorts::findFree(reserved, 25565);
        if (port < 0) {
            return fail(tr("Не нашлось свободного порта."));
        }
        config.port = port;
    }

    const QString dir = root.filePath(config.id);
    if (!QDir().mkpath(dir)) {
        return fail(tr("Не удалось создать папку сервера."));
    }
    if (!config.save(dir)) {
        return fail(tr("Не удалось сохранить настройки сервера."));
    }

    // server.properties with the assigned port, so two servers never fight for 25565
    ServerProperties props;
    const QString propsPath = QDir(dir).filePath(QStringLiteral("server.properties"));
    props.load(propsPath);
    props.setInt(QStringLiteral("server-port"), config.port);
    if (!props.save(propsPath)) {
        return fail(tr("Не удалось записать server.properties."));
    }

    if (config.eulaAccepted) {
        QFile eula(QDir(dir).filePath(QStringLiteral("eula.txt")));
        if (eula.open(QIODevice::WriteOnly | QIODevice::Text)) {
            eula.write("eula=true\n");
        }
    }

    auto* server = new ServerProcess(config, dir, this);
    attach(server);
    return server;
}

bool ServerManager::remove(const QString& id, QString* error)
{
    ServerProcess* server = find(id);
    if (!server) {
        return true;
    }
    if (server->isActive()) {
        if (error) {
            *error = tr("Сначала остановите сервер.");
        }
        return false;
    }
    if (!QDir(server->directory()).removeRecursively()) {
        if (error) {
            *error = tr("Не удалось удалить папку сервера.");
        }
        return false;
    }
    m_servers.erase(std::remove(m_servers.begin(), m_servers.end(), server), m_servers.end());
    server->deleteLater();
    emit serverRemoved(id);
    return true;
}

bool ServerManager::saveConfig(const QString& id, const ServerConfig& config, QString* error)
{
    ServerProcess* server = find(id);
    if (!server) {
        if (error) {
            *error = tr("Сервер не найден.");
        }
        return false;
    }
    ServerConfig updated = config;
    updated.id = id;
    if (reservedPorts(id).contains(updated.port)) {
        if (error) {
            *error = tr("Порт %1 уже занят другим сервером из списка.").arg(updated.port);
        }
        return false;
    }
    if (!updated.save(server->directory())) {
        if (error) {
            *error = tr("Не удалось сохранить настройки сервера.");
        }
        return false;
    }
    // keep server.properties in sync with the port
    ServerProperties props;
    const QString propsPath = QDir(server->directory()).filePath(QStringLiteral("server.properties"));
    props.load(propsPath);
    props.setInt(QStringLiteral("server-port"), updated.port);
    props.save(propsPath);
    server->setConfig(updated);
    return true;
}

int ServerManager::runningCount() const
{
    int n = 0;
    for (ServerProcess* s : m_servers) {
        if (s->isActive()) {
            ++n;
        }
    }
    return n;
}

int ServerManager::activeMemoryMb() const
{
    int total = 0;
    for (ServerProcess* s : m_servers) {
        if (s->isActive()) {
            total += s->config().memoryMb;
        }
    }
    return total;
}

void ServerManager::stopAll()
{
    for (ServerProcess* s : m_servers) {
        s->stop();
    }
}
