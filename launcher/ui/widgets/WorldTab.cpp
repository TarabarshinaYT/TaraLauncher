// SPDX-License-Identifier: GPL-3.0-only
#include "WorldTab.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

#include "servers/ServerProcess.h"
#include "servers/ServerProperties.h"

namespace {
bool copyDir(const QString& from, const QString& to)
{
    if (!QDir().mkpath(to)) {
        return false;
    }
    const QDir src(from);
    for (const QFileInfo& info : src.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden)) {
        const QString target = QDir(to).filePath(info.fileName());
        if (info.isDir()) {
            if (!copyDir(info.filePath(), target)) {
                return false;
            }
        } else if (!QFile::copy(info.filePath(), target)) {
            return false;
        }
    }
    return true;
}
}  // namespace

WorldTab::WorldTab(ServerProcess* server, QWidget* parent) : QWidget(parent), m_server(server)
{
    auto* layout = new QVBoxLayout(this);
    auto* info = new QLabel(tr("Копии мира хранятся в папке backups внутри папки сервера. Создавать копию, восстанавливать и сбрасывать мир можно только при остановленном сервере."), this);
    info->setWordWrap(true);
    layout->addWidget(info);

    auto* openRow = new QHBoxLayout();
    auto* openWorld = new QPushButton(tr("Открыть папку мира"), this);
    connect(openWorld, &QPushButton::clicked, this, [this] { QDesktopServices::openUrl(QUrl::fromLocalFile(worldPath())); });
    auto* create = new QPushButton(tr("Создать копию мира"), this);
    connect(create, &QPushButton::clicked, this, &WorldTab::createBackup);
    openRow->addWidget(openWorld);
    openRow->addWidget(create);
    layout->addLayout(openRow);

    layout->addWidget(new QLabel(tr("Копии:"), this));
    m_backups = new QListWidget(this);
    layout->addWidget(m_backups, 1);

    auto* row = new QHBoxLayout();
    auto* restore = new QPushButton(tr("Восстановить выбранную"), this);
    connect(restore, &QPushButton::clicked, this, &WorldTab::restoreBackup);
    auto* del = new QPushButton(tr("Удалить копию"), this);
    connect(del, &QPushButton::clicked, this, &WorldTab::deleteBackup);
    auto* reset = new QPushButton(tr("Сбросить мир (создать новый)"), this);
    connect(reset, &QPushButton::clicked, this, &WorldTab::resetWorld);
    row->addWidget(restore);
    row->addWidget(del);
    row->addWidget(reset);
    layout->addLayout(row);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    reloadBackups();
}

QString WorldTab::worldName() const
{
    ServerProperties props;
    props.load(QDir(m_server->directory()).filePath(QStringLiteral("server.properties")));
    return props.value(QStringLiteral("level-name"), QStringLiteral("world"));
}

QString WorldTab::worldPath() const
{
    return QDir(m_server->directory()).filePath(worldName());
}

QString WorldTab::backupsPath() const
{
    return QDir(m_server->directory()).filePath(QStringLiteral("backups"));
}

bool WorldTab::ensureStopped()
{
    if (m_server->isActive()) {
        QMessageBox::information(this, tr("Мир"), tr("Сначала остановите сервер."));
        return false;
    }
    return true;
}

void WorldTab::reloadBackups()
{
    m_backups->clear();
    for (const QFileInfo& f : QDir(backupsPath()).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time)) {
        m_backups->addItem(f.fileName());
    }
}

void WorldTab::createBackup()
{
    if (!ensureStopped()) {
        return;
    }
    if (!QDir(worldPath()).exists()) {
        m_status->setText(tr("Мира ещё нет: запустите сервер хотя бы один раз."));
        return;
    }
    setCursor(Qt::WaitCursor);
    const QString target = QDir(backupsPath()).filePath(QStringLiteral("%1_%2").arg(worldName(), QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"))));
    const bool ok = copyDir(worldPath(), target);
    unsetCursor();
    m_status->setText(ok ? tr("Копия создана: %1").arg(QFileInfo(target).fileName()) : tr("Не удалось скопировать мир (мало места или файлы заняты)."));
    reloadBackups();
}

void WorldTab::restoreBackup()
{
    auto* item = m_backups->currentItem();
    if (!item || !ensureStopped()) {
        return;
    }
    if (QMessageBox::question(this, tr("Восстановление"), tr("Текущий мир будет заменён копией «%1». Текущий мир при этом сохранится как копия «before-restore». Продолжить?").arg(item->text())) != QMessageBox::Yes) {
        return;
    }
    setCursor(Qt::WaitCursor);
    bool ok = true;
    if (QDir(worldPath()).exists()) {
        const QString safe = QDir(backupsPath()).filePath(QStringLiteral("before-restore_%1").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"))));
        ok = QDir().mkpath(backupsPath()) && QDir().rename(worldPath(), safe);
    }
    ok = ok && copyDir(QDir(backupsPath()).filePath(item->text()), worldPath());
    unsetCursor();
    m_status->setText(ok ? tr("Мир восстановлен из «%1».").arg(item->text()) : tr("Не удалось восстановить мир."));
    reloadBackups();
}

void WorldTab::deleteBackup()
{
    auto* item = m_backups->currentItem();
    if (!item) {
        return;
    }
    if (QMessageBox::question(this, tr("Удаление"), tr("Удалить копию «%1» навсегда?").arg(item->text())) != QMessageBox::Yes) {
        return;
    }
    QDir(QDir(backupsPath()).filePath(item->text())).removeRecursively();
    reloadBackups();
}

void WorldTab::resetWorld()
{
    if (!ensureStopped()) {
        return;
    }
    if (QMessageBox::warning(this, tr("Сброс мира"),
                             tr("Текущий мир будет перенесён в копию, а при следующем запуске сервер создаст новый мир. Продолжить?"),
                             QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        return;
    }
    if (!QDir(worldPath()).exists()) {
        m_status->setText(tr("Мира нет, сбрасывать нечего."));
        return;
    }
    QDir().mkpath(backupsPath());
    const QString safe = QDir(backupsPath()).filePath(QStringLiteral("reset_%1").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"))));
    const bool ok = QDir().rename(worldPath(), safe);
    m_status->setText(ok ? tr("Мир перенесён в копию «%1». Новый мир появится при запуске.").arg(QFileInfo(safe).fileName()) : tr("Не удалось перенести мир."));
    reloadBackups();
}
