// SPDX-License-Identifier: GPL-3.0-only
#include "ServersWindow.h"

#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <memory>

#include "DesktopServices.h"
#include "HardwareInfo.h"
#include "servers/ServerInstaller.h"
#include "servers/ServerJavaTask.h"
#include "servers/ServerManager.h"
#include "ui/dialogs/NewServerDialog.h"
#include "ui/dialogs/ProgressDialog.h"

namespace {
constexpr int kIdRole = Qt::UserRole + 1;
}

ServersWindow::ServersWindow(ServerManager* manager, QWidget* parent) : QWidget(parent, Qt::Window), m_manager(manager)
{
    setWindowTitle(tr("Серверы"));
    resize(980, 600);

    auto* root = new QHBoxLayout(this);

    // left column: list + actions
    auto* left = new QVBoxLayout();
    m_list = new QListWidget(this);
    m_list->setMinimumWidth(320);
    left->addWidget(m_list, 1);

    auto* create = new QPushButton(tr("Создать сервер..."), this);
    left->addWidget(create);
    auto* row1 = new QHBoxLayout();
    m_start = new QPushButton(tr("Запустить"), this);
    m_stop = new QPushButton(tr("Остановить"), this);
    m_restart = new QPushButton(tr("Перезапустить"), this);
    row1->addWidget(m_start);
    row1->addWidget(m_stop);
    row1->addWidget(m_restart);
    left->addLayout(row1);
    auto* row2 = new QHBoxLayout();
    m_folder = new QPushButton(tr("Папка"), this);
    m_delete = new QPushButton(tr("Удалить"), this);
    row2->addWidget(m_folder);
    row2->addWidget(m_delete);
    left->addLayout(row2);
    m_warning = new QLabel(this);
    m_warning->setWordWrap(true);
    left->addWidget(m_warning);
    root->addLayout(left);

    // right column: console
    auto* right = new QVBoxLayout();
    right->addWidget(new QLabel(tr("Консоль"), this));
    m_console = new QPlainTextEdit(this);
    m_console->setReadOnly(true);
    m_console->setMaximumBlockCount(5000);
    right->addWidget(m_console, 1);
    m_command = new QLineEdit(this);
    m_command->setPlaceholderText(tr("Команда сервера (например: say привет) и Enter"));
    right->addWidget(m_command);
    root->addLayout(right, 1);

    connect(create, &QPushButton::clicked, this, &ServersWindow::createServer);
    connect(m_start, &QPushButton::clicked, this, &ServersWindow::startSelected);
    connect(m_stop, &QPushButton::clicked, this, &ServersWindow::stopSelected);
    connect(m_restart, &QPushButton::clicked, this, &ServersWindow::restartSelected);
    connect(m_folder, &QPushButton::clicked, this, &ServersWindow::openFolder);
    connect(m_delete, &QPushButton::clicked, this, &ServersWindow::deleteSelected);
    connect(m_command, &QLineEdit::returnPressed, this, &ServersWindow::sendCommand);
    connect(m_list, &QListWidget::currentItemChanged, this, &ServersWindow::selectionChanged);
    connect(m_manager, &ServerManager::serverAdded, this, &ServersWindow::addItem);
    connect(m_manager, &ServerManager::serverRemoved, this, [this](const QString& id) {
        delete m_list->takeItem(m_list->row(itemFor(id)));
        updateButtons();
    });
    connect(m_manager, &ServerManager::serverStatusChanged, this, [this](ServerProcess* s, ServerProcess::Status) {
        refreshItem(s);
        updateButtons();
    });

    for (ServerProcess* s : m_manager->servers()) {
        addItem(s);
    }
    if (m_list->count() > 0) {
        m_list->setCurrentRow(0);
    }
    updateButtons();
}

QString ServersWindow::statusText(ServerProcess::Status status)
{
    switch (status) {
        case ServerProcess::Status::Stopped: return tr("остановлен");
        case ServerProcess::Status::Starting: return tr("запускается...");
        case ServerProcess::Status::Running: return tr("работает");
        case ServerProcess::Status::Stopping: return tr("останавливается...");
        case ServerProcess::Status::Crashed: return tr("сбой");
    }
    return QString();
}

ServerProcess* ServersWindow::selected() const
{
    auto* item = m_list->currentItem();
    return item ? m_manager->find(item->data(kIdRole).toString()) : nullptr;
}

QListWidgetItem* ServersWindow::itemFor(const QString& id) const
{
    for (int i = 0; i < m_list->count(); ++i) {
        if (m_list->item(i)->data(kIdRole).toString() == id) {
            return m_list->item(i);
        }
    }
    return nullptr;
}

void ServersWindow::addItem(ServerProcess* server)
{
    auto* item = new QListWidgetItem(m_list);
    item->setData(kIdRole, server->config().id);
    connect(server, &ServerProcess::logAppended, this, [this, server](const QStringList& lines) {
        if (selected() == server) {
            m_console->appendPlainText(lines.join('\n'));
        }
    });
    connect(server, &ServerProcess::failed, this, [this](const QString& reason) { QMessageBox::warning(this, tr("Сервер"), reason); });
    refreshItem(server);
}

void ServersWindow::refreshItem(ServerProcess* server)
{
    auto* item = itemFor(server->config().id);
    if (!item) {
        return;
    }
    const auto& c = server->config();
    item->setText(tr("%1\n%2 %3 · порт %4 · %5")
                      .arg(c.name, serverTypeName(c.type), c.minecraftVersion)
                      .arg(c.port)
                      .arg(statusText(server->status())));
}

void ServersWindow::selectionChanged()
{
    m_console->clear();
    if (auto* s = selected()) {
        m_console->setPlainText(s->logLines().join('\n'));
    }
    updateButtons();
}

void ServersWindow::updateButtons()
{
    auto* s = selected();
    const bool has = s != nullptr;
    const bool active = has && s->isActive();
    m_start->setEnabled(has && !active);
    m_stop->setEnabled(active && s->status() != ServerProcess::Status::Stopping);
    m_restart->setEnabled(active);
    m_folder->setEnabled(has);
    m_delete->setEnabled(has && !active);
    m_command->setEnabled(has && active);

    const int active_count = m_manager->runningCount();
    m_warning->setText(active_count > 0 ? tr("Запущено серверов: %1 (память под них: %2 МБ)").arg(active_count).arg(m_manager->activeMemoryMb())
                                        : QString());
}

void ServersWindow::createServer()
{
    NewServerDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    QString error;
    ServerProcess* server = m_manager->create(dialog.config(), &error);
    if (!server) {
        QMessageBox::warning(this, tr("Новый сервер"), error);
        return;
    }

    // download server.jar with a progress dialog; roll back the empty server on failure
    auto installer = std::make_unique<ServerInstaller>(server->config(), server->directory());
    ServerInstaller* raw = installer.get();
    ProgressDialog progress(this);
    const int result = progress.execWithTask(installer.get());
    const ServerConfig resultConfig = raw->resultConfig();
    if (result != QDialog::Accepted) {
        const QString reason = raw->failReason();
        QString removeError;
        m_manager->remove(server->config().id, &removeError);
        if (!reason.isEmpty()) {
            QMessageBox::warning(this, tr("Новый сервер"), reason);
        }
        return;
    }
    // remember what the installer learned (loader build, required Java version)
    QString saveError;
    ServerConfig installed = server->config();
    installed.loaderVersion = resultConfig.loaderVersion;
    installed.javaMajor = resultConfig.javaMajor;
    m_manager->saveConfig(server->config().id, installed, &saveError);
    refreshItem(server);
    if (auto* item = itemFor(server->config().id)) {
        m_list->setCurrentItem(item);
    }
}

void ServersWindow::startSelected()
{
    auto* s = selected();
    if (!s) {
        return;
    }
    const quint64 total = HardwareInfo::totalRamMiB();
    if (total > 0 && static_cast<quint64>(m_manager->activeMemoryMb() + s->config().memoryMb) > total * 8 / 10) {
        const auto answer = QMessageBox::question(
            this, tr("Мало памяти"),
            tr("Серверы вместе займут %1 МБ из %2 МБ памяти компьютера. Всё равно запустить?").arg(m_manager->activeMemoryMb() + s->config().memoryMb).arg(total));
        if (answer != QMessageBox::Yes) {
            return;
        }
    }
    if (!ensureJava(s)) {
        return;
    }
    m_console->clear();
    s->start();
    updateButtons();
}

bool ServersWindow::ensureJava(ServerProcess* s)
{
    // a Java chosen by the user (or found earlier) that still exists is used as it is
    if (!s->config().javaPath.isEmpty() && QFile::exists(s->config().javaPath)) {
        return true;
    }
    auto task = std::make_unique<ServerJavaTask>(s->config().requiredJavaMajor());
    ServerJavaTask* raw = task.get();
    ProgressDialog progress(this);
    const int result = progress.execWithTask(task.get());
    if (result != QDialog::Accepted) {
        const QString reason = raw->failReason();
        if (!reason.isEmpty()) {
            QMessageBox::warning(this, tr("Java"), tr("Не удалось подготовить Java %1: %2").arg(s->config().requiredJavaMajor()).arg(reason));
        }
        return false;
    }
    ServerConfig updated = s->config();
    updated.javaPath = raw->javaPath();
    QString error;
    m_manager->saveConfig(updated.id, updated, &error);
    return true;
}

void ServersWindow::stopSelected()
{
    if (auto* s = selected()) {
        s->stop();
    }
}

void ServersWindow::restartSelected()
{
    auto* s = selected();
    if (!s || !s->isActive()) {
        return;
    }
    // start again as soon as the server has really stopped
    auto connection = std::make_shared<QMetaObject::Connection>();
    *connection = connect(s, &ServerProcess::statusChanged, this, [this, s, connection](ServerProcess::Status status) {
        if (status == ServerProcess::Status::Stopped || status == ServerProcess::Status::Crashed) {
            QObject::disconnect(*connection);
            s->start();
        }
    });
    s->stop();
}

void ServersWindow::deleteSelected()
{
    auto* s = selected();
    if (!s) {
        return;
    }
    const auto answer = QMessageBox::warning(this, tr("Удалить сервер"),
                                             tr("Сервер «%1» будет удалён вместе с миром и всеми файлами. Это нельзя отменить. Удалить?").arg(s->config().name),
                                             QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }
    QString error;
    if (!m_manager->remove(s->config().id, &error)) {
        QMessageBox::warning(this, tr("Удалить сервер"), error);
    }
}

void ServersWindow::openFolder()
{
    if (auto* s = selected()) {
        DesktopServices::openPath(s->directory());
    }
}

void ServersWindow::sendCommand()
{
    auto* s = selected();
    const QString text = m_command->text().trimmed();
    if (!s || text.isEmpty()) {
        return;
    }
    s->sendCommand(text);
    m_command->clear();
}
