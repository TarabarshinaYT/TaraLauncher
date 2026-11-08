// SPDX-License-Identifier: GPL-3.0-only
#include "ConnectionTab.h"

#include <QApplication>
#include <QClipboard>
#include <QFrame>
#include <QHBoxLayout>
#include <QHostAddress>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkReply>
#include <QPushButton>
#include <QVBoxLayout>

#include "servers/ServerNetwork.h"
#include "servers/ServerProcess.h"
#include "ui/widgets/HelpIcon.h"

ConnectionTab::ConnectionTab(ServerProcess* server, QWidget* parent) : QWidget(parent), m_server(server), m_net(this)
{
    m_upnp = new UpnpClient(this);
    const int port = server->config().port;
    m_localIps = ServerNetwork::localAddresses();

    auto* layout = new QVBoxLayout(this);

    auto* intro = new QLabel(tr("Чтобы друзья из интернета могли зайти на сервер, нужно три вещи: разрешить порт в брандмауэре Windows, открыть его на роутере и дать друзьям правильный адрес. Ниже всё это можно сделать кнопками."), this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    layout->addWidget(addressRow(tr("Для игроков в вашей сети (Wi‑Fi, один роутер):"), m_local,
                                 tr("Этот адрес подойдёт тем, кто подключён к тому же роутеру, что и вы. Из интернета по нему зайти нельзя.")));
    QStringList withPort;
    for (const QString& ip : m_localIps) {
        withPort << QStringLiteral("%1:%2").arg(ip).arg(port);
    }
    m_local->setText(withPort.isEmpty() ? tr("не найден (нет подключения к сети)") : withPort.join(QStringLiteral("   "))) ;

    layout->addWidget(addressRow(tr("Для игроков из интернета:"), m_public,
                                 tr("Ваш публичный IP-адрес, который видят в интернете. Он работает только если порт открыт на роутере и у провайдера есть «белый» IP.")));
    m_public->setText(tr("определяю..."));

    // custom address
    auto* customRow = new QWidget(this);
    auto* customLayout = new QHBoxLayout(customRow);
    customLayout->setContentsMargins(0, 0, 0, 0);
    m_custom = new QLineEdit(server->config().publicAddress, customRow);
    m_custom->setPlaceholderText(tr("например: play.mysite.ru или 203.0.113.5:25570"));
    auto* copyCustom = new QPushButton(tr("Копировать"), customRow);
    connect(copyCustom, &QPushButton::clicked, this, [this] {
        if (!m_custom->text().trimmed().isEmpty()) {
            copy(m_custom->text().trimmed());
        }
    });
    customLayout->addWidget(m_custom, 1);
    customLayout->addWidget(copyCustom);
    auto* customTitle = new QLabel(tr("Свой адрес сервера (необязательно):"), this);
    layout->addWidget(customTitle);
    layout->addWidget(HelpIcon::wrap(customRow,
                                     tr("Здесь можно записать красивый адрес, который вы дадите друзьям, например свой домен или адрес от сервиса вроде playit.gg / ngrok.\n"
                                        "Важно: лаунчер НЕ создаёт домен сам. Это просто подпись: она показывается в списке серверов и копируется одной кнопкой.\n"
                                        "Чтобы домен реально работал, у него должна быть запись A на ваш публичный IP (или SRV-запись с портом) у регистратора домена."),
                                     this));

    // diagnosis
    m_diagnosis = new QLabel(this);
    m_diagnosis->setWordWrap(true);
    m_diagnosis->setTextFormat(Qt::RichText);
    layout->addWidget(m_diagnosis);

    // actions
    auto* firewall = new QPushButton(tr("1. Разрешить порт %1 в брандмауэре Windows").arg(port), this);
    connect(firewall, &QPushButton::clicked, this, &ConnectionTab::openFirewall);
    layout->addWidget(HelpIcon::wrap(firewall,
                                     tr("Windows блокирует входящие подключения к Java при первом запуске, если вы нажали «Отмена» в окне брандмауэра. Кнопка создаёт разрешающее правило для порта.\n"
                                        "Появится запрос прав администратора, его нужно подтвердить."),
                                     this));

    auto* routerRow = new QWidget(this);
    auto* routerLayout = new QHBoxLayout(routerRow);
    routerLayout->setContentsMargins(0, 0, 0, 0);
    m_upnpOpen = new QPushButton(tr("2. Открыть порт %1 на роутере (UPnP)").arg(port), routerRow);
    m_upnpClose = new QPushButton(tr("Закрыть"), routerRow);
    connect(m_upnpOpen, &QPushButton::clicked, this, [this] { openRouterPort(true); });
    connect(m_upnpClose, &QPushButton::clicked, this, [this] { openRouterPort(false); });
    routerLayout->addWidget(m_upnpOpen, 1);
    routerLayout->addWidget(m_upnpClose);
    layout->addWidget(HelpIcon::wrap(routerRow,
                                     tr("Просит роутер автоматически «пробросить» порт к этому компьютеру. Работает, если на роутере включён UPnP.\n"
                                        "Если не получилось: зайдите в настройки роутера (обычно 192.168.0.1 или 192.168.1.1), найдите «Port Forwarding / Виртуальные серверы» и пробросьте TCP-порт %1 на адрес вашего компьютера.").arg(port),
                                     this));

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    layout->addStretch(1);

    connect(m_upnp, &UpnpClient::progress, this, [this](const QString& text) { m_status->setText(text); });
    connect(m_upnp, &UpnpClient::finished, this, [this](bool ok, const QString& message, const QString& externalIp) {
        m_status->setText(message);
        m_upnpOpen->setEnabled(true);
        m_upnpClose->setEnabled(true);
        if (ok && !externalIp.isEmpty()) {
            m_routerIp = externalIp;
            updateDiagnosis();
        }
    });

    updateDiagnosis();
    fetchPublicIp();
}

QWidget* ConnectionTab::addressRow(const QString& title, QLabel*& valueLabel, const QString& help)
{
    auto* box = new QWidget(this);
    auto* v = new QVBoxLayout(box);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(2);
    v->addWidget(new QLabel(title, box));

    auto* row = new QWidget(box);
    auto* h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    valueLabel = new QLabel(row);
    valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QFont f = valueLabel->font();
    f.setBold(true);
    valueLabel->setFont(f);
    auto* copyButton = new QPushButton(tr("Копировать"), row);
    QLabel* label = valueLabel;
    connect(copyButton, &QPushButton::clicked, this, [this, label] {
        // copy only the first address of the line
        copy(label->text().split(QLatin1Char(' '), Qt::SkipEmptyParts).value(0));
    });
    h->addWidget(valueLabel, 1);
    h->addWidget(copyButton);
    v->addWidget(HelpIcon::wrap(row, help, box));
    return box;
}

QString ConnectionTab::publicAddress() const
{
    return m_custom->text().trimmed();
}

void ConnectionTab::copy(const QString& text)
{
    QApplication::clipboard()->setText(text);
    m_status->setText(tr("Скопировано: %1").arg(text));
}

void ConnectionTab::fetchPublicIp()
{
    QNetworkRequest request(QUrl(QStringLiteral("https://api.ipify.org")));
    request.setTransferTimeout(8000);
    QNetworkReply* reply = m_net.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_public->setText(tr("не удалось определить (нет интернета?)"));
            return;
        }
        setPublicIp(QString::fromUtf8(reply->readAll()).trimmed());
    });
}

void ConnectionTab::setPublicIp(const QString& ip)
{
    if (QHostAddress(ip).isNull()) {
        m_public->setText(tr("не удалось определить"));
        return;
    }
    m_publicIp = ip;
    m_public->setText(QStringLiteral("%1:%2").arg(ip).arg(m_server->config().port));
    updateDiagnosis();
}

void ConnectionTab::updateDiagnosis()
{
    QString text;
    const QHostAddress router(m_routerIp);
    if (!m_routerIp.isEmpty() && ServerNetwork::isPrivate(router)) {
        text = tr("<b>⚠ Подключиться из интернета, скорее всего, не получится.</b> Роутер сам получает «серый» адрес %1 от провайдера (CGNAT). "
                  "Порт открыть невозможно. Варианты: попросить у провайдера «белый» (выделенный) IP, использовать туннель (playit.gg, ngrok) или "
                  "игру через Radmin VPN / ZeroTier / Tailscale.").arg(m_routerIp);
    } else if (!m_routerIp.isEmpty() && !m_publicIp.isEmpty() && m_routerIp != m_publicIp) {
        text = tr("<b>⚠ Адрес роутера (%1) отличается от публичного (%2).</b> Между вами и интернетом ещё один NAT, порт, открытый на роутере, снаружи не виден.")
                   .arg(m_routerIp, m_publicIp);
    } else if (!m_routerIp.isEmpty()) {
        text = tr("✔ Роутер имеет публичный адрес %1, порт можно открыть.").arg(m_routerIp);
    } else {
        text = tr("Нажмите «Открыть порт на роутере»: заодно лаунчер проверит, есть ли у вас «белый» IP.");
    }
    m_diagnosis->setText(text);
}

void ConnectionTab::openFirewall()
{
    QString error;
    setCursor(Qt::WaitCursor);
    const bool ok = ServerNetwork::addFirewallRule(m_server->config().port, &error);
    unsetCursor();
    m_status->setText(ok ? tr("Правило брандмауэра создано для порта %1.").arg(m_server->config().port) : error);
}

void ConnectionTab::openRouterPort(bool open)
{
    m_upnpOpen->setEnabled(false);
    m_upnpClose->setEnabled(false);
    if (open) {
        m_upnp->forwardPort(m_server->config().port, QStringLiteral("TaraLauncher Minecraft"));
    } else {
        m_upnp->removePort(m_server->config().port);
    }
}
