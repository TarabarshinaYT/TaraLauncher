// SPDX-License-Identifier: GPL-3.0-only
#include "ContentTab.h"

#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QNetworkReply>
#include <QPushButton>
#include <QSaveFile>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>

#include "Application.h"
#include "servers/ServerProcess.h"

namespace {
constexpr int kIdRole = Qt::UserRole;
constexpr char kApi[] = "https://api.modrinth.com/v2";

QNetworkRequest makeRequest(const QUrl& url)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, APPLICATION->getUserAgent().toUtf8());
    request.setTransferTimeout(20000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    return request;
}

QString jsonArg(const QStringList& values)
{
    return QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(values)).toJson(QJsonDocument::Compact));
}
}  // namespace

ContentTab::ContentTab(ServerProcess* server, QWidget* parent) : QWidget(parent), m_server(server), m_net(this)
{
    auto* layout = new QVBoxLayout(this);
    const ServerConfig& c = server->config();
    const bool plugins = serverTypeUsesPlugins(c.type);

    if (loaders().isEmpty()) {
        auto* note = new QLabel(tr("Для обычного (Vanilla) сервера моды и плагины не подходят. Создайте сервер типа Fabric (моды) или Paper/Purpur (плагины)."), this);
        note->setWordWrap(true);
        layout->addWidget(note);
        layout->addStretch(1);
        return;
    }

    auto* info = new QLabel(tr("Поиск на Modrinth для %1 %2. Устанавливаются файлы, совместимые с этой версией. Обязательные зависимости (например, Fabric API) нужно ставить отдельно.")
                                .arg(serverTypeName(c.type), c.minecraftVersion),
                            this);
    info->setWordWrap(true);
    layout->addWidget(info);

    auto* row = new QHBoxLayout();
    m_query = new QLineEdit(this);
    m_query->setPlaceholderText(plugins ? tr("Название плагина, например: LuckPerms") : tr("Название мода, например: Lithium"));
    auto* go = new QPushButton(tr("Найти"), this);
    row->addWidget(m_query, 1);
    row->addWidget(go);
    layout->addLayout(row);
    connect(m_query, &QLineEdit::returnPressed, this, &ContentTab::search);
    connect(go, &QPushButton::clicked, this, &ContentTab::search);

    m_results = new QListWidget(this);
    layout->addWidget(m_results, 2);
    m_install = new QPushButton(plugins ? tr("Установить выбранный плагин") : tr("Установить выбранный мод"), this);
    m_install->setEnabled(false);
    layout->addWidget(m_install);
    connect(m_results, &QListWidget::itemSelectionChanged, this, [this] { m_install->setEnabled(!m_results->selectedItems().isEmpty()); });
    connect(m_install, &QPushButton::clicked, this, &ContentTab::installSelected);

    layout->addWidget(new QLabel(plugins ? tr("Установленные плагины (папка plugins):") : tr("Установленные моды (папка mods):"), this));
    m_installed = new QListWidget(this);
    layout->addWidget(m_installed, 1);
    auto* remove = new QPushButton(tr("Удалить выбранный файл"), this);
    layout->addWidget(remove);
    connect(remove, &QPushButton::clicked, this, &ContentTab::removeSelected);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    reloadInstalled();
}

QStringList ContentTab::loaders() const
{
    switch (m_server->config().type) {
        case ServerType::Fabric:
            return { "fabric" };
        case ServerType::Quilt:
            return { "quilt", "fabric" };
        case ServerType::Forge:
            return { "forge" };
        case ServerType::NeoForge:
            return { "neoforge" };
        case ServerType::Paper:
            return { "paper", "spigot", "bukkit" };
        case ServerType::Purpur:
            return { "purpur", "paper", "spigot", "bukkit" };
        default:
            return {};
    }
}

QString ContentTab::contentDir() const
{
    return QDir(m_server->directory()).filePath(serverTypeUsesPlugins(m_server->config().type) ? QStringLiteral("plugins") : QStringLiteral("mods"));
}

void ContentTab::setStatus(const QString& text)
{
    m_status->setText(text);
}

void ContentTab::search()
{
    const QStringList loaderList = loaders();
    QJsonArray loaderFacet;
    for (const QString& l : loaderList) {
        loaderFacet.append(QStringLiteral("categories:%1").arg(l));
    }
    QJsonArray facets;
    facets.append(QJsonArray{ QStringLiteral("project_type:mod") });
    facets.append(loaderFacet);
    facets.append(QJsonArray{ QStringLiteral("versions:%1").arg(m_server->config().minecraftVersion) });

    QUrl url(QString::fromLatin1(kApi) + QStringLiteral("/search"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("query"), m_query->text().trimmed());
    q.addQueryItem(QStringLiteral("facets"), QString::fromUtf8(QJsonDocument(facets).toJson(QJsonDocument::Compact)));
    q.addQueryItem(QStringLiteral("limit"), QStringLiteral("30"));
    url.setQuery(q);

    setStatus(tr("Ищу на Modrinth..."));
    QNetworkReply* reply = m_net.get(makeRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            setStatus(tr("Ошибка поиска: %1").arg(reply->errorString()));
            return;
        }
        m_results->clear();
        const auto hits = QJsonDocument::fromJson(reply->readAll()).object().value("hits").toArray();
        for (const auto& h : hits) {
            const QJsonObject o = h.toObject();
            auto* item = new QListWidgetItem(tr("%1 — %2\n   скачиваний: %3").arg(o.value("title").toString(), o.value("description").toString()).arg(o.value("downloads").toInt()));
            item->setData(kIdRole, o.value("project_id").toString());
            item->setData(kIdRole + 1, o.value("title").toString());
            m_results->addItem(item);
        }
        setStatus(hits.isEmpty() ? tr("Ничего не найдено для этой версии.") : tr("Найдено: %1").arg(hits.size()));
    });
}

void ContentTab::installSelected()
{
    auto* item = m_results->currentItem();
    if (!item) {
        return;
    }
    const QString projectId = item->data(kIdRole).toString();
    const QString title = item->data(kIdRole + 1).toString();

    QUrl url(QString::fromLatin1(kApi) + QStringLiteral("/project/%1/version").arg(projectId));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("loaders"), jsonArg(loaders()));
    q.addQueryItem(QStringLiteral("game_versions"), jsonArg({ m_server->config().minecraftVersion }));
    url.setQuery(q);

    m_install->setEnabled(false);
    setStatus(tr("Ищу подходящую версию «%1»...").arg(title));
    QNetworkReply* reply = m_net.get(makeRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, title] {
        reply->deleteLater();
        auto fail = [this](const QString& text) {
            setStatus(text);
            m_install->setEnabled(!m_results->selectedItems().isEmpty());
        };
        if (reply->error() != QNetworkReply::NoError) {
            return fail(tr("Ошибка: %1").arg(reply->errorString()));
        }
        const auto versions = QJsonDocument::fromJson(reply->readAll()).array();
        if (versions.isEmpty()) {
            return fail(tr("Для «%1» нет подходящего файла.").arg(title));
        }
        const auto files = versions.first().toObject().value("files").toArray();
        QJsonObject file;
        for (const auto& f : files) {
            if (f.toObject().value("primary").toBool()) {
                file = f.toObject();
            }
        }
        if (file.isEmpty() && !files.isEmpty()) {
            file = files.first().toObject();
        }
        const QString fileName = QFileInfo(file.value("filename").toString()).fileName();  // strip any path
        const QUrl downloadUrl(file.value("url").toString());
        if (fileName.isEmpty() || !downloadUrl.isValid() || downloadUrl.scheme() != QLatin1String("https")) {
            return fail(tr("Modrinth вернул некорректный файл."));
        }
        setStatus(tr("Скачиваю %1...").arg(fileName));
        QNetworkReply* download = m_net.get(makeRequest(downloadUrl));
        connect(download, &QNetworkReply::finished, this, [this, download, fileName, title] {
            download->deleteLater();
            m_install->setEnabled(!m_results->selectedItems().isEmpty());
            if (download->error() != QNetworkReply::NoError) {
                setStatus(tr("Ошибка загрузки: %1").arg(download->errorString()));
                return;
            }
            QDir().mkpath(contentDir());
            QSaveFile out(QDir(contentDir()).filePath(fileName));
            if (!out.open(QIODevice::WriteOnly) || out.write(download->readAll()) < 0 || !out.commit()) {
                setStatus(tr("Не удалось сохранить файл %1.").arg(fileName));
                return;
            }
            setStatus(tr("«%1» установлен (%2). Перезапустите сервер, чтобы он загрузился.").arg(title, fileName));
            reloadInstalled();
        });
    });
}

void ContentTab::reloadInstalled()
{
    m_installed->clear();
    const QFileInfoList files = QDir(contentDir()).entryInfoList({ QStringLiteral("*.jar"), QStringLiteral("*.jar.disabled") }, QDir::Files, QDir::Name);
    for (const QFileInfo& f : files) {
        m_installed->addItem(f.fileName());
    }
}

void ContentTab::removeSelected()
{
    auto* item = m_installed->currentItem();
    if (!item) {
        return;
    }
    if (QMessageBox::question(this, tr("Удаление"), tr("Удалить файл %1?").arg(item->text())) != QMessageBox::Yes) {
        return;
    }
    if (!QFile::remove(QDir(contentDir()).filePath(item->text()))) {
        setStatus(tr("Не удалось удалить файл (возможно, сервер запущен и использует его)."));
        return;
    }
    reloadInstalled();
}
