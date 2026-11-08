// SPDX-License-Identifier: GPL-3.0-only
#include "ServerNotifier.h"

#include <QApplication>
#include <QSystemTrayIcon>

#include "Application.h"
#include "ServerManager.h"

namespace {
bool flag(const char* key)
{
    return APPLICATION->settings()->get(QString::fromLatin1(key)).toBool();
}
void setFlag(const char* key, bool on)
{
    APPLICATION->settings()->set(QString::fromLatin1(key), on);
}
}  // namespace

bool ServerNotifier::joinEnabled() { return flag("ServerNotifyJoin"); }
bool ServerNotifier::crashEnabled() { return flag("ServerNotifyCrash"); }
bool ServerNotifier::errorEnabled() { return flag("ServerNotifyError"); }
void ServerNotifier::setJoinEnabled(bool on) { setFlag("ServerNotifyJoin", on); }
void ServerNotifier::setCrashEnabled(bool on) { setFlag("ServerNotifyCrash", on); }
void ServerNotifier::setErrorEnabled(bool on) { setFlag("ServerNotifyError", on); }

ServerNotifier::ServerNotifier(ServerManager* manager, QObject* parent) : QObject(parent)
{
    connect(manager, &ServerManager::playerJoined, this, [this](ServerProcess* server, const QString& name) {
        if (joinEnabled()) {
            notify(server->config().name, tr("Игрок %1 зашёл на сервер").arg(name), false);
        }
    });
    connect(manager, &ServerManager::serverCrashed, this, [this](ServerProcess* server) {
        if (crashEnabled()) {
            notify(server->config().name, tr("Сервер неожиданно остановился (сбой)"), true);
        }
    });
    connect(manager, &ServerManager::serverErrorLogged, this, [this](ServerProcess* server, const QString& line) {
        if (errorEnabled()) {
            notify(server->config().name, tr("Ошибка в журнале сервера: %1").arg(line.left(160)), true);
        }
    });
}

void ServerNotifier::notify(const QString& title, const QString& text, bool warning)
{
    if (!QSystemTrayIcon::isSystemTrayAvailable() || !QSystemTrayIcon::supportsMessages()) {
        return;
    }
    if (!m_tray) {
        // The tray icon is what lets the system show a toast; it is created only when the first notification is needed.
        m_tray = new QSystemTrayIcon(QApplication::windowIcon(), this);
        m_tray->setToolTip(tr("TaraLauncher: серверы"));
        m_tray->show();
    }
    m_tray->showMessage(title, text, warning ? QSystemTrayIcon::Warning : QSystemTrayIcon::Information, 6000);
}
