// SPDX-License-Identifier: GPL-3.0-only
#include "ServerNetwork.h"

#include <QNetworkInterface>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

namespace ServerNetwork {

bool isCarrierGradeNat(const QHostAddress& address)
{
    return address.isInSubnet(QHostAddress(QStringLiteral("100.64.0.0")), 10);
}

bool isPrivate(const QHostAddress& address)
{
    return address.isInSubnet(QHostAddress(QStringLiteral("10.0.0.0")), 8) || address.isInSubnet(QHostAddress(QStringLiteral("172.16.0.0")), 12) ||
           address.isInSubnet(QHostAddress(QStringLiteral("192.168.0.0")), 16) || address.isInSubnet(QHostAddress(QStringLiteral("169.254.0.0")), 16) ||
           isCarrierGradeNat(address);
}

QStringList localAddresses()
{
    static const QRegularExpression virtualAdapter(QStringLiteral("vEthernet|VirtualBox|VMware|Hyper-V|WSL|Docker|VPN|Loopback|Bluetooth"),
                                                   QRegularExpression::CaseInsensitiveOption);
    QStringList out;
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        const auto flags = iface.flags();
        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning) || (flags & QNetworkInterface::IsLoopBack)) {
            continue;
        }
        if (virtualAdapter.match(iface.humanReadableName()).hasMatch()) {
            continue;
        }
        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            const QHostAddress ip = entry.ip();
            if (ip.protocol() != QAbstractSocket::IPv4Protocol || ip.isLoopback() || ip.isInSubnet(QHostAddress(QStringLiteral("169.254.0.0")), 16)) {
                continue;
            }
            out << ip.toString();
        }
    }
    // home networks first: 192.168.x.x, then 10.x.x.x, then the rest
    std::stable_sort(out.begin(), out.end(), [](const QString& a, const QString& b) {
        auto rank = [](const QString& s) { return s.startsWith(QLatin1String("192.168.")) ? 0 : (s.startsWith(QLatin1String("10.")) ? 1 : 2); };
        return rank(a) < rank(b);
    });
    out.removeDuplicates();
    return out;
}

bool addFirewallRule(int port, QString* error)
{
#ifdef Q_OS_WIN
    const QString name = QStringLiteral("TaraLauncher Minecraft %1").arg(port);
    // delete an old rule of the same name first, so pressing the button twice does not create duplicates
    const QString params = QStringLiteral(
                               "/c netsh advfirewall firewall delete rule name=\"%1\" >nul 2>&1 & "
                               "netsh advfirewall firewall add rule name=\"%1\" dir=in action=allow protocol=TCP localport=%2")
                               .arg(name)
                               .arg(port);
    const std::wstring parameters = params.toStdWString();

    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS;
    info.lpVerb = L"runas";  // asks for administrator rights
    info.lpFile = L"cmd.exe";
    info.lpParameters = parameters.c_str();
    info.nShow = SW_HIDE;
    if (!ShellExecuteExW(&info)) {
        if (error) {
            *error = GetLastError() == ERROR_CANCELLED ? QObject::tr("Вы отказались дать права администратора, правило не создано.")
                                                       : QObject::tr("Не удалось запустить настройку брандмауэра.");
        }
        return false;
    }
    bool ok = false;
    if (info.hProcess) {
        if (WaitForSingleObject(info.hProcess, 20000) == WAIT_OBJECT_0) {
            DWORD code = 1;
            GetExitCodeProcess(info.hProcess, &code);
            ok = code == 0;
        }
        CloseHandle(info.hProcess);
    }
    if (!ok && error) {
        *error = QObject::tr("Брандмауэр не принял правило (команда завершилась с ошибкой).");
    }
    return ok;
#else
    Q_UNUSED(port);
    if (error) {
        *error = QObject::tr("Правило брандмауэра создаётся только в Windows. На Linux откройте порт вручную (ufw или firewalld).");
    }
    return false;
#endif
}

}  // namespace ServerNetwork

// ---------------------------------------------------------------------------------------------- UPnP

namespace {
constexpr int kDiscoveryTimeoutMs = 4000;

QString xmlEscape(const QString& s)
{
    return s.toHtmlEscaped();
}
}  // namespace

UpnpClient::UpnpClient(QObject* parent) : QObject(parent), m_socket(this), m_timeout(this), m_net(this)
{
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        finish(false, tr("Роутер не ответил. Возможно, на нём выключен UPnP, он его не поддерживает или перед вами ещё один роутер (например, у провайдера)."));
    });
    connect(&m_socket, &QUdpSocket::readyRead, this, &UpnpClient::onDatagram);
}

void UpnpClient::forwardPort(int port, const QString& description)
{
    start(Action::Forward, port, description);
}

void UpnpClient::removePort(int port)
{
    start(Action::Remove, port, QString());
}

void UpnpClient::start(Action action, int port, const QString& description)
{
    if (m_busy) {
        return;
    }
    m_busy = true;
    m_action = action;
    m_port = port;
    m_description = description;
    m_controlUrl.clear();
    discover();
}

void UpnpClient::finish(bool ok, const QString& message, const QString& externalIp)
{
    if (!m_busy) {
        return;
    }
    m_busy = false;
    m_timeout.stop();
    m_socket.close();
    emit finished(ok, message, externalIp);
}

void UpnpClient::discover()
{
    emit progress(tr("Ищу роутер в сети..."));
    m_socket.close();
    if (!m_socket.bind(QHostAddress::AnyIPv4, 0)) {
        finish(false, tr("Не удалось открыть сетевой порт для поиска роутера."));
        return;
    }
    const QByteArray request =
        "M-SEARCH * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "MAN: \"ssdp:discover\"\r\n"
        "MX: 2\r\n"
        "ST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n\r\n";
    m_socket.writeDatagram(request, QHostAddress(QStringLiteral("239.255.255.250")), 1900);
    m_timeout.start(kDiscoveryTimeoutMs);
}

void UpnpClient::onDatagram()
{
    while (m_socket.hasPendingDatagrams()) {
        QHostAddress sender;
        QByteArray data;
        data.resize(static_cast<int>(m_socket.pendingDatagramSize()));
        m_socket.readDatagram(data.data(), data.size(), &sender);
        if (!m_busy || m_controlUrl.isValid()) {
            continue;
        }
        static const QRegularExpression location(QStringLiteral("^LOCATION:\\s*(\\S+)\\s*$"),
                                                 QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption);
        const auto match = location.match(QString::fromLatin1(data));
        if (!match.hasMatch()) {
            continue;
        }
        const QUrl url(match.captured(1));
        if (!url.isValid()) {
            continue;
        }
        m_timeout.stop();
        m_router = sender;
        m_socket.close();  // one router is enough, stop reading
        fetchDescription(url);
        return;
    }
}

void UpnpClient::fetchDescription(const QUrl& location)
{
    emit progress(tr("Роутер найден (%1), читаю его описание...").arg(m_router.toString()));
    QNetworkRequest request(location);
    request.setTransferTimeout(5000);
    QNetworkReply* reply = m_net.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, location] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            finish(false, tr("Не удалось прочитать описание роутера: %1").arg(reply->errorString()));
            return;
        }
        QUrl base = location;
        QString bestType;
        QString bestControl;
        QXmlStreamReader xml(reply->readAll());
        QString currentType;
        QString currentControl;
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement()) {
                const QString tag = xml.name().toString();
                if (tag == QLatin1String("URLBase")) {
                    const QUrl declared(xml.readElementText());
                    if (declared.isValid() && !declared.isEmpty()) {
                        base = declared;
                    }
                } else if (tag == QLatin1String("service")) {
                    currentType.clear();
                    currentControl.clear();
                } else if (tag == QLatin1String("serviceType")) {
                    currentType = xml.readElementText().trimmed();
                } else if (tag == QLatin1String("controlURL")) {
                    currentControl = xml.readElementText().trimmed();
                }
            } else if (xml.isEndElement() && xml.name() == QLatin1String("service")) {
                const bool wan = currentType.contains(QLatin1String("WANIPConnection")) || currentType.contains(QLatin1String("WANPPPConnection"));
                // prefer WANIPConnection over the older PPP variant
                if (wan && !currentControl.isEmpty() && (bestType.isEmpty() || currentType.contains(QLatin1String("WANIPConnection")))) {
                    bestType = currentType;
                    bestControl = currentControl;
                }
            }
        }
        if (bestControl.isEmpty()) {
            finish(false, tr("Роутер отвечает, но не умеет открывать порты через UPnP."));
            return;
        }
        m_serviceType = bestType;
        m_controlUrl = base.resolved(QUrl(bestControl));

        // which of this computer's addresses is in the router's network?
        const QStringList local = ServerNetwork::localAddresses();
        m_localIp.clear();
        for (const QString& ip : local) {
            if (QHostAddress(ip).isInSubnet(m_router, 24)) {
                m_localIp = ip;
                break;
            }
        }
        if (m_localIp.isEmpty() && !local.isEmpty()) {
            m_localIp = local.first();
        }
        if (m_localIp.isEmpty()) {
            finish(false, tr("Не удалось определить адрес этого компьютера в сети."));
            return;
        }
        runAction();
    });
}

void UpnpClient::callRouter(const QString& action, const QString& arguments, std::function<void(bool, const QByteArray&)> done)
{
    const QString body = QStringLiteral(
                             "<?xml version=\"1.0\"?>"
                             "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
                             "<s:Body><u:%1 xmlns:u=\"%2\">%3</u:%1></s:Body></s:Envelope>")
                             .arg(action, m_serviceType, arguments);
    QNetworkRequest request(m_controlUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("text/xml; charset=\"utf-8\""));
    request.setRawHeader("SOAPAction", QStringLiteral("\"%1#%2\"").arg(m_serviceType, action).toUtf8());
    request.setTransferTimeout(6000);
    QNetworkReply* reply = m_net.post(request, body.toUtf8());
    connect(reply, &QNetworkReply::finished, this, [reply, done = std::move(done)] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        done(reply->error() == QNetworkReply::NoError && status == 200, reply->readAll());
    });
}

void UpnpClient::runAction()
{
    auto tag = [](const QString& name, const QString& value) { return QStringLiteral("<%1>%2</%1>").arg(name, xmlEscape(value)); };

    if (m_action == Action::Remove) {
        emit progress(tr("Закрываю порт %1 на роутере...").arg(m_port));
        const QString args = tag("NewRemoteHost", QString()) + tag("NewExternalPort", QString::number(m_port)) + tag("NewProtocol", "TCP");
        callRouter(QStringLiteral("DeletePortMapping"), args, [this](bool ok, const QByteArray&) {
            finish(ok, ok ? tr("Порт закрыт на роутере.") : tr("Роутер не смог закрыть порт (возможно, он уже был закрыт)."));
        });
        return;
    }

    emit progress(tr("Открываю порт %1 на роутере для адреса %2...").arg(m_port).arg(m_localIp));
    auto addMapping = [this, tag](int leaseSeconds, std::function<void(bool, const QByteArray&)> done) {
        const QString args = tag("NewRemoteHost", QString()) + tag("NewExternalPort", QString::number(m_port)) + tag("NewProtocol", "TCP") +
                             tag("NewInternalPort", QString::number(m_port)) + tag("NewInternalClient", m_localIp) + tag("NewEnabled", "1") +
                             tag("NewPortMappingDescription", m_description) + tag("NewLeaseDuration", QString::number(leaseSeconds));
        callRouter(QStringLiteral("AddPortMapping"), args, std::move(done));
    };

    // some routers accept only permanent rules (lease 0), others only timed ones: try both
    addMapping(0, [this, addMapping, tag](bool ok, const QByteArray&) {
        auto afterMapping = [this](bool mapped) {
            if (!mapped) {
                finish(false, tr("Роутер отказался открыть порт. Проверьте, что UPnP включён в настройках роутера, или откройте порт вручную."));
                return;
            }
            // ask the router for its external address, to tell the user whether the internet can reach it at all
            callRouter(QStringLiteral("GetExternalIPAddress"), QString(), [this](bool got, const QByteArray& body) {
                QString ip;
                if (got) {
                    static const QRegularExpression re(QStringLiteral("<NewExternalIPAddress>([^<]*)</NewExternalIPAddress>"));
                    ip = re.match(QString::fromUtf8(body)).captured(1).trimmed();
                }
                finish(true, tr("Порт %1 открыт на роутере.").arg(m_port), ip);
            });
        };
        if (ok) {
            afterMapping(true);
            return;
        }
        addMapping(86400, [afterMapping](bool okTimed, const QByteArray&) { afterMapping(okTimed); });
    });
}
