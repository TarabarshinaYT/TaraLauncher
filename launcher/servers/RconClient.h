// SPDX-License-Identifier: GPL-3.0-only
/*
 *  TaraLauncher - Minecraft Launcher
 *  Minimal Minecraft RCON client (Source RCON protocol). Used to send commands and
 *  to stop servers that keep running independently of the launcher.
 */
#pragma once

#include <QString>

namespace Rcon {
/**
 * Connects to 127.0.0.1:port, logs in and runs one command.
 * Blocking with short timeouts (a couple of seconds at most).
 * `reply` receives the server's answer (may be empty, e.g. for "stop").
 */
bool send(int port, const QString& password, const QString& command, QString* reply = nullptr, QString* error = nullptr);
/** Random password for a new server. */
QString makePassword();
}  // namespace Rcon
