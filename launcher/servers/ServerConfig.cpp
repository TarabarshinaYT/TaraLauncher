// SPDX-License-Identifier: GPL-3.0-only
#include "ServerConfig.h"

#include <QDir>
#include <QObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QFile>
#include <QRegularExpression>

QString serverTypeId(ServerType type)
{
    switch (type) {
        case ServerType::Vanilla: return "vanilla";
        case ServerType::Paper: return "paper";
        case ServerType::Purpur: return "purpur";
        case ServerType::Fabric: return "fabric";
        case ServerType::Quilt: return "quilt";
        case ServerType::Forge: return "forge";
        case ServerType::NeoForge: return "neoforge";
    }
    return "vanilla";
}

QString serverTypeName(ServerType type)
{
    switch (type) {
        case ServerType::Vanilla: return QObject::tr("Ванилла");
        case ServerType::Paper: return "Paper";
        case ServerType::Purpur: return "Purpur";
        case ServerType::Fabric: return "Fabric";
        case ServerType::Quilt: return "Quilt";
        case ServerType::Forge: return "Forge";
        case ServerType::NeoForge: return "NeoForge";
    }
    return QString();
}

ServerType serverTypeFromId(const QString& id, bool* ok)
{
    static const ServerType all[] = { ServerType::Vanilla, ServerType::Paper, ServerType::Purpur, ServerType::Fabric,
                                      ServerType::Quilt,   ServerType::Forge, ServerType::NeoForge };
    for (ServerType t : all) {
        if (serverTypeId(t) == id) {
            if (ok) {
                *ok = true;
            }
            return t;
        }
    }
    if (ok) {
        *ok = false;
    }
    return ServerType::Vanilla;
}

bool serverTypeUsesPlugins(ServerType type)
{
    return type == ServerType::Paper || type == ServerType::Purpur;
}

bool serverTypeUsesMods(ServerType type)
{
    return type == ServerType::Fabric || type == ServerType::Quilt || type == ServerType::Forge || type == ServerType::NeoForge;
}

int ServerConfig::guessJavaMajor(const QString& minecraftVersion)
{
    // Accepts "1.16.5", "1.20", "1.21.4" and year-based versions such as "26.2".
    static const QRegularExpression re(QStringLiteral("^(\\d+)\\.(\\d+)(?:\\.(\\d+))?"));
    const auto m = re.match(minecraftVersion);
    if (!m.hasMatch()) {
        return 21;
    }
    const int major = m.captured(1).toInt();
    const int minor = m.captured(2).toInt();
    const int patch = m.captured(3).isEmpty() ? 0 : m.captured(3).toInt();
    if (major >= 26) {
        return 25;  // assumption for year-based versions; the real value comes from Mojang's version manifest
    }
    if (major == 1) {
        if (minor <= 16) {
            return 8;
        }
        if (minor == 17) {
            return 16;
        }
        if (minor < 20 || (minor == 20 && patch <= 4)) {
            return 17;
        }
        return 21;
    }
    return 21;
}

bool ServerConfig::load(const QString& dir)
{
    QFile file(QDir(dir).filePath(fileName()));
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }
    const QJsonObject o = doc.object();
    id = QDir(dir).dirName();
    name = o.value("name").toString(id);
    minecraftVersion = o.value("minecraftVersion").toString();
    bool ok = false;
    type = serverTypeFromId(o.value("type").toString(), &ok);
    loaderVersion = o.value("loaderVersion").toString();
    port = o.value("port").toInt(25565);
    memoryMb = o.value("memoryMb").toInt(2048);
    javaPath = o.value("javaPath").toString();
    javaMajor = o.value("javaMajor").toInt(0);
    eulaAccepted = o.value("eulaAccepted").toBool(false);
    rconPort = o.value("rconPort").toInt(0);
    rconPassword = o.value("rconPassword").toString();
    publicAddress = o.value("publicAddress").toString();
    extraJvmArgs.clear();
    for (const auto& v : o.value("extraJvmArgs").toArray()) {
        extraJvmArgs << v.toString();
    }
    launchArgs.clear();
    for (const auto& v : o.value("launchArgs").toArray()) {
        launchArgs << v.toString();
    }
    return !minecraftVersion.isEmpty();
}

bool ServerConfig::save(const QString& dir) const
{
    QDir().mkpath(dir);
    QJsonObject o;
    o["name"] = name;
    o["minecraftVersion"] = minecraftVersion;
    o["type"] = serverTypeId(type);
    o["loaderVersion"] = loaderVersion;
    o["port"] = port;
    o["memoryMb"] = memoryMb;
    o["javaPath"] = javaPath;
    o["javaMajor"] = javaMajor;
    o["eulaAccepted"] = eulaAccepted;
    o["rconPort"] = rconPort;
    o["rconPassword"] = rconPassword;
    o["publicAddress"] = publicAddress;
    o["extraJvmArgs"] = QJsonArray::fromStringList(extraJvmArgs);
    o["launchArgs"] = QJsonArray::fromStringList(launchArgs);

    QSaveFile file(QDir(dir).filePath(fileName()));
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    return file.commit();
}
