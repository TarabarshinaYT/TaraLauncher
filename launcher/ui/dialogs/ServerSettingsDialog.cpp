// SPDX-License-Identifier: GPL-3.0-only
#include "ServerSettingsDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include "servers/ServerManager.h"
#include "ui/widgets/HelpIcon.h"
#include "ui/widgets/MapTab.h"
#include "ui/widgets/PlayersTab.h"
#include "ui/widgets/ServerPropertiesEditor.h"

ServerSettingsDialog::ServerSettingsDialog(ServerManager* manager, ServerProcess* server, QWidget* parent)
    : QDialog(parent), m_manager(manager), m_server(server)
{
    setWindowTitle(tr("Настройки сервера: %1").arg(server->config().name));
    resize(760, 640);

    auto* layout = new QVBoxLayout(this);
    m_tabs = new QTabWidget(this);
    layout->addWidget(m_tabs, 1);

    m_tabs->addTab(buildGeneralTab(), tr("Основное"));

    m_properties = new ServerPropertiesEditor(server->config().name, this);
    m_properties->load(QDir(server->directory()).filePath(QStringLiteral("server.properties")));
    m_tabs->addTab(m_properties, tr("server.properties"));
    m_tabs->addTab(new PlayersTab(server, this), tr("Игроки"));
    m_tabs->addTab(new MapTab(server, this), tr("Карта"));

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Save)->setText(tr("Сохранить"));
    buttons->button(QDialogButtonBox::Close)->setText(tr("Закрыть"));
    connect(buttons, &QDialogButtonBox::accepted, this, &ServerSettingsDialog::save);
    connect(buttons->button(QDialogButtonBox::Save), &QPushButton::clicked, this, &ServerSettingsDialog::save);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Close), &QPushButton::clicked, this, &QDialog::reject);
    layout->addWidget(buttons);
}

QWidget* ServerSettingsDialog::buildGeneralTab()
{
    const ServerConfig& c = m_server->config();
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    form->addRow(tr("Тип и версия:"), new QLabel(QStringLiteral("%1 %2").arg(serverTypeName(c.type), c.minecraftVersion), page));

    m_name = new QLineEdit(c.name, page);
    form->addRow(tr("Название:"), HelpIcon::wrap(m_name, tr("Как сервер называется в списке лаунчера. На игру это не влияет."), page));

    m_port = new QSpinBox(page);
    m_port->setRange(1024, 65535);
    m_port->setValue(c.port);
    m_port->setButtonSymbols(QAbstractSpinBox::PlusMinus);
    form->addRow(tr("Порт:"), HelpIcon::wrap(m_port, tr("Порт, по которому игроки подключаются: адрес:порт. У каждого сервера должен быть свой порт, стандартный 25565.\n"
                                                         "Если изменить порт, игрокам придётся использовать новый адрес."), page));

    m_memory = new QSpinBox(page);
    m_memory->setRange(512, 262144);
    m_memory->setSingleStep(256);
    m_memory->setSuffix(tr(" МБ"));
    m_memory->setValue(c.memoryMb);
    m_memory->setButtonSymbols(QAbstractSpinBox::PlusMinus);
    form->addRow(tr("Память:"), HelpIcon::wrap(m_memory, tr("Сколько оперативной памяти выделить серверу. Слишком мало приводит к лагам и вылетам, слишком много оставит мало памяти другим программам.\n"
                                                             "Для чистого сервера обычно хватает 2–4 ГБ, для сервера с модами нужно 4–8 ГБ."), page));

    auto* javaRow = new QWidget(page);
    auto* javaLayout = new QHBoxLayout(javaRow);
    javaLayout->setContentsMargins(0, 0, 0, 0);
    m_java = new QLineEdit(c.javaPath, javaRow);
    m_java->setPlaceholderText(tr("Автоматически (нужна Java %1)").arg(c.requiredJavaMajor()));
    auto* browse = new QPushButton(tr("Выбрать..."), javaRow);
    connect(browse, &QPushButton::clicked, this, &ServerSettingsDialog::browseJava);
    javaLayout->addWidget(m_java, 1);
    javaLayout->addWidget(browse);
    form->addRow(tr("Java:"), HelpIcon::wrap(javaRow, tr("Путь к java (или java.exe). Если оставить пустым, лаунчер сам найдёт подходящую версию или скачает её.\n"
                                                         "Для разных версий Minecraft нужны разные Java: 1.16.5 и старше обычно требует Java 8, 1.17–1.20.4 Java 17, а новее Java 21 и выше."), page));

    m_jvmArgs = new QLineEdit(c.extraJvmArgs.join(' '), page);
    form->addRow(tr("Аргументы JVM:"), HelpIcon::wrap(m_jvmArgs, tr("Дополнительные параметры запуска Java, например флаги сборщика мусора. Если не знаете, что это, оставьте пустым: неверные значения могут помешать запуску сервера."), page));

    m_eula = new QCheckBox(tr("Я принимаю EULA Minecraft"), page);
    m_eula->setChecked(c.eulaAccepted);
    form->addRow(QString(), HelpIcon::wrap(m_eula, tr("Лицензионное соглашение Mojang (aka.ms/MinecraftEULA). Без согласия сервер не запустится."), page));

    return page;
}

void ServerSettingsDialog::browseJava()
{
    const QString file = QFileDialog::getOpenFileName(this, tr("Выберите java"), m_java->text());
    if (!file.isEmpty()) {
        m_java->setText(file);
    }
}

void ServerSettingsDialog::save()
{
    ServerConfig config = m_server->config();
    config.name = m_name->text().trimmed().isEmpty() ? config.name : m_name->text().trimmed();
    config.port = m_port->value();
    config.memoryMb = m_memory->value();
    config.javaPath = m_java->text().trimmed();
    config.extraJvmArgs = QProcess::splitCommand(m_jvmArgs->text());
    config.eulaAccepted = m_eula->isChecked();

    QString error;
    if (!m_manager->saveConfig(config.id, config, &error)) {
        QMessageBox::warning(this, tr("Сохранение"), error);
        return;
    }
    // eula.txt follows the checkbox so the server sees it too
    if (config.eulaAccepted) {
        QFile eula(QDir(m_server->directory()).filePath(QStringLiteral("eula.txt")));
        if (eula.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            eula.write("eula=true\n");
        }
    }
    if (!m_properties->save()) {
        QMessageBox::warning(this, tr("Сохранение"), tr("Не удалось записать server.properties."));
        return;
    }
    if (m_server->isActive()) {
        QMessageBox::information(this, tr("Сохранено"), tr("Настройки сохранены. Они применятся после перезапуска сервера."));
    }
}
