// SPDX-License-Identifier: GPL-3.0-only
/*
 *  TaraLauncher - Minecraft Launcher
 *  Server component: per-server configuration stored in tara-server.json
 */
#pragma once

#include <QString>
#include <QStringList>

enum class ServerType { Vanilla, Paper, Purpur, Fabric, Quilt, Forge, NeoForge };

QString serverTypeId(ServerType type);
/** Human-readable (Russian) name for the UI. */
QString serverTypeName(ServerType type);
ServerType serverTypeFromId(const QString& id, bool* ok = nullptr);
/** Plugin servers use /plugins, mod servers use /mods. */
bool serverTypeUsesPlugins(ServerType type);
bool serverTypeUsesMods(ServerType type);

struct ServerConfig {
    QString id;               // directory name, unique
    QString name;             // display name
    QString minecraftVersion; // e.g. "1.16.5"
    ServerType type = ServerType::Vanilla;
    QString loaderVersion;    // loader/build version, empty for vanilla
    int port = 25565;
    int memoryMb = 2048;
    QString javaPath;         // empty = pick automatically
    int javaMajor = 0;        // required Java major version, 0 = guess from the Minecraft version
    QStringList extraJvmArgs;
    QStringList launchArgs;   // arguments after JVM flags, e.g. {"-jar", "server.jar", "nogui"}
    bool eulaAccepted = false;
    int rconPort = 0;         // 0 = not assigned yet
    QString rconPassword;     // generated on first start
    QString publicAddress;    // address shown to players (domain or IP, optional :port); empty = detect automatically

    /** Best guess of the required Java major version, used until the real value is read from Mojang's manifest. */
    static int guessJavaMajor(const QString& minecraftVersion);
    int requiredJavaMajor() const { return javaMajor > 0 ? javaMajor : guessJavaMajor(minecraftVersion); }

    bool load(const QString& dir);
    bool save(const QString& dir) const;

    static QString fileName() { return QStringLiteral("tara-server.json"); }
};
