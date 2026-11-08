// SPDX-License-Identifier: GPL-3.0-only
/*
 *  TaraLauncher - Minecraft Launcher
 *  Desktop notifications (Windows toast / macOS / Linux) about server events.
 */
#pragma once

#include <QObject>

class QSystemTrayIcon;
class ServerManager;
class ServerProcess;

class ServerNotifier : public QObject {
    Q_OBJECT
   public:
    explicit ServerNotifier(ServerManager* manager, QObject* parent = nullptr);

    static bool joinEnabled();
    static bool crashEnabled();
    static bool errorEnabled();
    static void setJoinEnabled(bool on);
    static void setCrashEnabled(bool on);
    static void setErrorEnabled(bool on);

   private:
    void notify(const QString& title, const QString& text, bool warning);

    QSystemTrayIcon* m_tray = nullptr;
};
