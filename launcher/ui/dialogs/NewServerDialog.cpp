// SPDX-License-Identifier: GPL-3.0-only
#include "NewServerDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include "Application.h"
#include "Filter.h"
#include "meta/Index.h"
#include "meta/VersionList.h"
#include "servers/ServerInstaller.h"
#include "ui/widgets/VersionSelectWidget.h"

NewServerDialog::NewServerDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Новый сервер"));
    resize(720, 600);

    auto* layout = new QVBoxLayout(this);

    // --- Minecraft version list, same widget and filters as in "New Instance" ---
    layout->addWidget(new QLabel(tr("Версия Minecraft"), this));
    m_versions = new VersionSelectWidget(this);
    layout->addWidget(m_versions, 1);

    auto* filterRow = new QHBoxLayout();
    m_releases = new QCheckBox(tr("Релизы"), this);
    m_releases->setChecked(true);
    m_snapshots = new QCheckBox(tr("Снапшоты"), this);
    auto* refresh = new QPushButton(tr("Обновить"), this);
    filterRow->addWidget(m_releases);
    filterRow->addWidget(m_snapshots);
    filterRow->addStretch(1);
    filterRow->addWidget(refresh);
    layout->addLayout(filterRow);

    // --- Server settings ---
    auto* form = new QFormLayout();
    m_type = new QComboBox(this);
    const ServerType types[] = { ServerType::Vanilla, ServerType::Paper,  ServerType::Purpur,   ServerType::Fabric,
                                 ServerType::Quilt,   ServerType::Forge, ServerType::NeoForge };
    for (ServerType t : types) {
        QString label = serverTypeName(t);
        if (serverTypeUsesPlugins(t)) {
            label += tr(" (плагины)");
        } else if (serverTypeUsesMods(t)) {
            label += tr(" (моды)");
        }
        const bool supported = ServerInstaller::isSupported(t);
        if (!supported) {
            label += tr(" (скоро)");
        }
        m_type->addItem(label, serverTypeId(t));
        if (!supported) {
            // grey out types that cannot be installed yet
            if (auto* model = qobject_cast<QStandardItemModel*>(m_type->model())) {
                model->item(m_type->count() - 1)->setEnabled(false);
            }
        }
    }
    form->addRow(tr("Тип сервера:"), m_type);

    m_name = new QLineEdit(this);
    m_name->setPlaceholderText(tr("Название сервера"));
    form->addRow(tr("Название:"), m_name);

    m_memory = new QSpinBox(this);
    m_memory->setRange(512, 65536);
    m_memory->setSingleStep(256);
    m_memory->setValue(2048);
    m_memory->setSuffix(tr(" МБ"));
    form->addRow(tr("Память:"), m_memory);
    layout->addLayout(form);

    m_eula = new QCheckBox(tr("Я принимаю EULA Minecraft (https://aka.ms/MinecraftEULA)"), this);
    layout->addWidget(m_eula);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_buttons->button(QDialogButtonBox::Ok)->setText(tr("Создать"));
    m_buttons->button(QDialogButtonBox::Cancel)->setText(tr("Отмена"));
    layout->addWidget(m_buttons);

    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_versions, &VersionSelectWidget::selectedVersionChanged, this, &NewServerDialog::versionChanged);
    connect(m_releases, &QCheckBox::checkStateChanged, this, &NewServerDialog::filterChanged);
    connect(m_snapshots, &QCheckBox::checkStateChanged, this, &NewServerDialog::filterChanged);
    connect(refresh, &QPushButton::clicked, this, [this] { m_versions->loadList(true); });
    connect(m_type, &QComboBox::currentIndexChanged, this, &NewServerDialog::typeChanged);
    connect(m_eula, &QCheckBox::toggled, this, &NewServerDialog::updateOkButton);
    connect(m_name, &QLineEdit::textEdited, this, [this] { m_nameEditedByUser = true; });

    filterChanged();
    auto list = APPLICATION->metadataIndex()->get("net.minecraft");
    m_versions->initialize(list.get());
    updateOkButton();
}

void NewServerDialog::filterChanged()
{
    QStringList out;
    if (m_releases->isChecked()) {
        out << "(release)";
    }
    if (m_snapshots->isChecked()) {
        out << "(snapshot)";
    }
    // with nothing ticked show nothing instead of everything
    const QString pattern = out.isEmpty() ? QStringLiteral("(?!)") : out.join('|');
    m_versions->setFilter(BaseVersionList::TypeRole, Filters::regexp(QRegularExpression(pattern)));
}

void NewServerDialog::versionChanged(BaseVersion::Ptr version)
{
    m_selected = std::move(version);
    if (m_selected && !m_nameEditedByUser) {
        m_name->setText(tr("Сервер %1").arg(m_selected->descriptor()));
    }
    updateOkButton();
}

void NewServerDialog::typeChanged()
{
    // Fabric/Quilt servers need 1.14+, the same limit the client dialog applies.
    updateOkButton();
}

void NewServerDialog::updateOkButton()
{
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(m_selected != nullptr && m_eula->isChecked());
}

ServerConfig NewServerDialog::config() const
{
    ServerConfig c;
    c.name = m_name->text().trimmed();
    c.minecraftVersion = m_selected ? m_selected->descriptor() : QString();
    c.type = serverTypeFromId(m_type->currentData().toString());
    c.memoryMb = m_memory->value();
    c.eulaAccepted = m_eula->isChecked();
    c.port = 25565;
    return c;
}
