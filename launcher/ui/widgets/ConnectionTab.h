// SPDX-License-Identifier: GPL-3.0-only
/*
 *  TaraLauncher - Minecraft Launcher
 *  "Connection" tab: server addresses, custom address, firewall rule and router port forwarding.
 */
#pragma once

#include <QNetworkAccessManager>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QVBoxLayout;
class ServerProcess;
class UpnpClient;

class ConnectionTab : public QWidget {
    Q_OBJECT
   public:
    explicit ConnectionTab(ServerProcess* server, QWidget* parent = nullptr);

    /** Address typed by the user, empty = automatic. */
    QString publicAddress() const;

   private:
    QWidget* addressRow(const QString& title, QLabel*& valueLabel, const QString& help);
    void fetchPublicIp();
    void setPublicIp(const QString& ip);
    void openFirewall();
    void openRouterPort(bool open);
    void copy(const QString& text);
    void updateDiagnosis();

    ServerProcess* m_server = nullptr;
    QNetworkAccessManager m_net;
    UpnpClient* m_upnp = nullptr;
    QLabel* m_local = nullptr;
    QLabel* m_public = nullptr;
    QLabel* m_diagnosis = nullptr;
    QLabel* m_status = nullptr;
    QLineEdit* m_custom = nullptr;
    QPushButton* m_upnpOpen = nullptr;
    QPushButton* m_upnpClose = nullptr;
    QString m_publicIp;
    QString m_routerIp;
    QStringList m_localIps;
};
