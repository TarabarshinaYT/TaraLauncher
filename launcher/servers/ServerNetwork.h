// SPDX-License-Identifier: GPL-3.0-only
/*
 *  TaraLauncher - Minecraft Launcher
 *  Helpers that make a server reachable for other players: addresses, firewall rule, router port forwarding (UPnP).
 */
#pragma once

#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QUdpSocket>
#include <QUrl>
#include <functional>

namespace ServerNetwork {

/** IPv4 addresses of this computer in the local network, most likely home-network address first. */
QStringList localAddresses();

/** True for addresses that cannot be reached from the internet: 10/8, 172.16/12, 192.168/16, 169.254/16 and 100.64/10 (provider-level NAT). */
bool isPrivate(const QHostAddress& address);
/** True for 100.64.0.0/10, the range providers use for carrier-grade NAT. */
bool isCarrierGradeNat(const QHostAddress& address);

/**
 * Adds an inbound TCP rule for the port to Windows Firewall. Asks for administrator rights (UAC prompt),
 * blocks until the rule is created, the user declines, or 20 seconds pass. Windows only.
 */
bool addFirewallRule(int port, QString* error);

}  // namespace ServerNetwork

/** Talks to the home router over UPnP (SSDP discovery + SOAP) to open a port. */
class UpnpClient : public QObject {
    Q_OBJECT
   public:
    explicit UpnpClient(QObject* parent = nullptr);

    /** Finds the router, forwards TCP `port` to this computer and reads the router's external IP. */
    void forwardPort(int port, const QString& description);
    /** Removes the forwarding created by forwardPort(). */
    void removePort(int port);

   signals:
    void progress(const QString& text);
    /** `externalIp` is empty if the router did not tell. */
    void finished(bool ok, const QString& message, const QString& externalIp);

   private:
    enum class Action { Forward, Remove };

    void start(Action action, int port, const QString& description);
    void discover();
    void onDatagram();
    void fetchDescription(const QUrl& location);
    void runAction();
    void callRouter(const QString& action, const QString& arguments, std::function<void(bool, const QByteArray&)> done);
    void finish(bool ok, const QString& message, const QString& externalIp = QString());

    QUdpSocket m_socket;
    QTimer m_timeout;
    QNetworkAccessManager m_net;
    Action m_action = Action::Forward;
    int m_port = 0;
    QString m_description;
    QUrl m_controlUrl;
    QString m_serviceType;
    QHostAddress m_router;
    QString m_localIp;
    bool m_busy = false;
};
