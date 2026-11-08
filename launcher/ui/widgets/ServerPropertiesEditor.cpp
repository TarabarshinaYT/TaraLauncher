// SPDX-License-Identifier: GPL-3.0-only
#include "ServerPropertiesEditor.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include <vector>

#include "ui/dialogs/MotdEditorDialog.h"
#include "ui/widgets/HelpIcon.h"

namespace {

enum class Kind { Bool, Int, Text, Choice, Motd };

struct Option {
    const char* value;
    const char* label;
};

struct PropDef {
    const char* group;
    const char* key;
    const char* label;
    Kind kind;
    const char* def;  // default value as written in server.properties
    long long min;
    long long max;
    std::vector<Option> options;
    const char* help;
};

// Keys the launcher manages itself, so they are hidden from the "other settings" box.
const char* const kManagedKeys[] = { "server-port", "enable-rcon", "rcon.port", "rcon.password", "broadcast-rcon-to-ops", "query.port" };

const std::vector<PropDef>& definitions()
{
    static const std::vector<PropDef> defs = {
        // ---------------- Основные ----------------
        { "Основные", "motd", "Описание (MOTD)", Kind::Motd, "A Minecraft Server", 0, 0, {},
          "Текст под названием сервера в списке серверов. Нажмите «Редактор», чтобы выбрать цвета и сразу увидеть, как это будет выглядеть." },
        { "Основные", "max-players", "Максимум игроков", Kind::Int, "20", 1, 100000, {},
          "Сколько игроков могут быть на сервере одновременно. Лишние игроки не смогут зайти. Чем больше игроков, тем больше нужно памяти и процессора." },
        { "Основные", "online-mode", "Проверка лицензии игроков", Kind::Bool, "true", 0, 0, {},
          "Сервер проверяет у Mojang, что игрок вошёл под своим настоящим аккаунтом.\nЕсли выключить, любой сможет зайти под любым ником, в том числе под ником администратора, поэтому выключайте только для закрытой локальной сети и при полном доверии игрокам." },
        { "Основные", "white-list", "Белый список", Kind::Bool, "false", 0, 0, {},
          "Если включено, зайти могут только игроки из списка whitelist.json. Добавить игрока: команда whitelist add Ник в консоли." },
        { "Основные", "enforce-whitelist", "Выгонять тех, кого нет в белом списке", Kind::Bool, "false", 0, 0, {},
          "При включённом белом списке сразу выгоняет игроков, которых в нём нет, даже если они уже на сервере." },
        { "Основные", "pvp", "Бои между игроками (PvP)", Kind::Bool, "true", 0, 0, {},
          "Если выключено, игроки не могут наносить урон друг другу." },
        { "Основные", "difficulty", "Сложность", Kind::Choice, "easy", 0, 0,
          { { "peaceful", "Мирная" }, { "easy", "Лёгкая" }, { "normal", "Нормальная" }, { "hard", "Сложная" } },
          "Влияет на урон от мобов, голод и появление враждебных существ. На мирной нет врагов, а здоровье восстанавливается само." },
        { "Основные", "gamemode", "Режим игры по умолчанию", Kind::Choice, "survival", 0, 0,
          { { "survival", "Выживание" }, { "creative", "Творческий" }, { "adventure", "Приключение" }, { "spectator", "Наблюдатель" } },
          "В каком режиме игроки появляются при первом входе на сервер." },
        { "Основные", "force-gamemode", "Принудительно ставить режим игры", Kind::Bool, "false", 0, 0, {},
          "Если включено, при каждом входе игроку выставляется режим из настройки выше. Иначе игрок возвращается в том режиме, в котором вышел." },
        { "Основные", "hardcore", "Хардкор", Kind::Bool, "false", 0, 0, {},
          "Сложность закрепляется на «Сложная», а после смерти игрок переходит в режим наблюдателя." },
        { "Основные", "op-permission-level", "Права операторов", Kind::Int, "4", 1, 4, {},
          "Уровень прав у новых операторов: 1 только обход защиты спавна, 2 команды управления миром, 3 управление игроками, 4 все команды, включая остановку сервера." },

        // ---------------- Мир ----------------
        { "Мир", "level-name", "Название мира (папка)", Kind::Text, "world", 0, 0, {},
          "Имя папки, где хранится мир. Если указать несуществующее имя, при запуске создастся новый мир, а старый останется на диске." },
        { "Мир", "level-seed", "Зерно мира", Kind::Text, "", 0, 0, {},
          "Число или слово, по которому генерируется мир. Пусто значит случайное. Действует только при создании нового мира, у существующего менять бесполезно." },
        { "Мир", "level-type", "Тип мира", Kind::Text, "minecraft:normal", 0, 0, {},
          "В новых версиях: minecraft:normal, minecraft:flat (плоский), minecraft:large_biomes, minecraft:amplified. В старых (до 1.19): default, flat, largeBiomes, amplified. Действует при создании нового мира." },
        { "Мир", "generate-structures", "Создавать структуры", Kind::Bool, "true", 0, 0, {},
          "Деревни, крепости, шахты и другие постройки. Действует при создании нового мира." },
        { "Мир", "allow-nether", "Разрешить Нижний мир", Kind::Bool, "true", 0, 0, {},
          "Если выключено, порталы в Нижний мир не работают." },
        { "Мир", "spawn-monsters", "Появление монстров", Kind::Bool, "true", 0, 0, {},
          "Если выключено, враждебные существа не появляются сами." },
        { "Мир", "spawn-animals", "Появление животных", Kind::Bool, "true", 0, 0, {},
          "Если выключено, новые животные не появляются сами (существующие остаются)." },
        { "Мир", "spawn-npcs", "Появление жителей", Kind::Bool, "true", 0, 0, {},
          "Если выключено, в деревнях не появляются жители." },
        { "Мир", "spawn-protection", "Защита вокруг спавна (радиус)", Kind::Int, "16", 0, 29999984, {},
          "В этом радиусе вокруг точки возрождения обычные игроки не могут ломать и ставить блоки (операторы могут). Ноль отключает защиту." },
        { "Мир", "max-world-size", "Максимальный размер мира (радиус)", Kind::Int, "29999984", 1, 29999984, {},
          "Граница мира в блоках от центра. Меньший мир занимает меньше места на диске." },
        { "Мир", "allow-flight", "Разрешить полёт", Kind::Bool, "false", 0, 0, {},
          "Если выключено, игроков, которые «летают» без разрешения, выгоняют. Включите, если у вас моды или плагины с полётом." },
        { "Мир", "enable-command-block", "Командные блоки", Kind::Bool, "false", 0, 0, {},
          "Разрешает работу командных блоков." },

        // ---------------- Производительность ----------------
        { "Производительность", "view-distance", "Дальность прорисовки (чанки)", Kind::Int, "10", 2, 32, {},
          "Сколько чанков вокруг игрока сервер отправляет ему. Меньше значит меньше нагрузка на процессор и сеть. Для слабого ПК попробуйте 6–8." },
        { "Производительность", "simulation-distance", "Дальность симуляции (чанки)", Kind::Int, "10", 3, 32, {},
          "В каком радиусе вокруг игрока работают мобы, растут растения и тикают механизмы. Меньше значит быстрее сервер." },
        { "Производительность", "entity-broadcast-range-percentage", "Дальность видимости существ (%)", Kind::Int, "100", 10, 1000, {},
          "Насколько далеко игрок видит существ. 100 это стандарт, меньшие значения снижают нагрузку на сеть." },
        { "Производительность", "network-compression-threshold", "Порог сжатия пакетов", Kind::Int, "256", -1, 1048576, {},
          "Пакеты больше этого размера (в байтах) сжимаются. Минус один отключает сжатие, для локальной сети это может немного разгрузить процессор." },
        { "Производительность", "sync-chunk-writes", "Синхронная запись чанков", Kind::Bool, "true", 0, 0, {},
          "Безопаснее при сбое питания или падении, но чуть медленнее. Выключайте, если важна скорость и есть резервные копии." },
        { "Производительность", "max-tick-time", "Время зависания до остановки (мс)", Kind::Int, "60000", -1, 2147483647, {},
          "Если один игровой такт длится дольше, сервер считает, что завис, и останавливается. Минус один отключает эту защиту, это полезно для очень тяжёлых модпаков." },

        // ---------------- Сеть и прочее ----------------
        { "Сеть и прочее", "server-ip", "IP-адрес сервера", Kind::Text, "", 0, 0, {},
          "К какому адресу привязать сервер. Оставьте пустым, это правильно почти всегда. Порт сервера лаунчер задаёт сам в разделе «Основное»." },
        { "Сеть и прочее", "enable-status", "Показывать сервер в списке", Kind::Bool, "true", 0, 0, {},
          "Если выключено, сервер не отвечает на запросы списка серверов, игроки видят его как «недоступен», но зайти по адресу всё равно могут." },
        { "Сеть и прочее", "hide-online-players", "Скрывать список игроков", Kind::Bool, "false", 0, 0, {},
          "В списке серверов не показываются ники игроков, которые сейчас на сервере." },
        { "Сеть и прочее", "player-idle-timeout", "Выгонять за бездействие (минуты)", Kind::Int, "0", 0, 100000, {},
          "Через сколько минут без действий игрока выгонят. Ноль отключает." },
        { "Сеть и прочее", "enforce-secure-profile", "Требовать подписанный чат", Kind::Bool, "true", 0, 0, {},
          "Игроки должны иметь подписанный профиль Mojang для чата (1.19+). Отключите, если игроки не могут писать в чат." },
        { "Сеть и прочее", "log-ips", "Писать IP игроков в журнал", Kind::Bool, "true", 0, 0, {},
          "IP-адреса игроков попадают в логи сервера. Отключите, если не хотите хранить эти данные." },
        { "Сеть и прочее", "prevent-proxy-connections", "Блокировать прокси и VPN", Kind::Bool, "false", 0, 0, {},
          "Сервер отклоняет игроков, чей провайдер Mojang считает прокси. Иногда блокирует и честных игроков." },
        { "Сеть и прочее", "resource-pack", "Ресурспак (ссылка)", Kind::Text, "", 0, 0, {},
          "Прямая ссылка на ZIP-файл ресурспака. Игрокам при входе предложат его скачать." },
        { "Сеть и прочее", "require-resource-pack", "Ресурспак обязателен", Kind::Bool, "false", 0, 0, {},
          "Игрок, отказавшийся от ресурспака, будет выгнан." },
    };
    return defs;
}

}  // namespace

ServerPropertiesEditor::ServerPropertiesEditor(const QString& serverName, QWidget* parent) : QWidget(parent), m_serverName(serverName)
{
    build(serverName);
}

void ServerPropertiesEditor::markModified()
{
    if (m_loading) {
        return;
    }
    m_modified = true;
    emit modified();
}

void ServerPropertiesEditor::build(const QString& serverName)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    auto* note = new QLabel(tr("Изменения вступят в силу после перезапуска сервера."), this);
    note->setWordWrap(true);
    root->addWidget(note);

    auto* tabs = new QTabWidget(this);
    root->addWidget(tabs, 1);

    QMap<QString, QFormLayout*> forms;
    for (const PropDef& def : definitions()) {
        const QString group = QString::fromUtf8(def.group);
        if (!forms.contains(group)) {
            auto* page = new QWidget();
            auto* form = new QFormLayout(page);
            form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
            auto* scroll = new QScrollArea(this);
            scroll->setWidgetResizable(true);
            scroll->setFrameShape(QFrame::NoFrame);
            scroll->setWidget(page);
            tabs->addTab(scroll, tr(def.group));
            forms.insert(group, form);
        }
        QFormLayout* form = forms[group];
        const QString key = QString::fromUtf8(def.key);
        const QString help = QString::fromUtf8(def.help) + QStringLiteral("\n\n(") + key + QStringLiteral(")");
        QWidget* field = nullptr;

        switch (def.kind) {
            case Kind::Bool: {
                auto* box = new QCheckBox(this);
                connect(box, &QCheckBox::toggled, this, &ServerPropertiesEditor::markModified);
                field = box;
                break;
            }
            case Kind::Int: {
                auto* spin = new QSpinBox(this);
                spin->setRange(static_cast<int>(def.min), static_cast<int>(qMin<long long>(def.max, 2147483647)));
                spin->setButtonSymbols(QAbstractSpinBox::PlusMinus);
                connect(spin, &QSpinBox::valueChanged, this, &ServerPropertiesEditor::markModified);
                field = spin;
                break;
            }
            case Kind::Choice: {
                auto* combo = new QComboBox(this);
                for (const Option& o : def.options) {
                    combo->addItem(tr(o.label), QString::fromUtf8(o.value));
                }
                connect(combo, &QComboBox::currentIndexChanged, this, &ServerPropertiesEditor::markModified);
                field = combo;
                break;
            }
            case Kind::Text: {
                auto* edit = new QLineEdit(this);
                connect(edit, &QLineEdit::textEdited, this, &ServerPropertiesEditor::markModified);
                field = edit;
                break;
            }
            case Kind::Motd: {
                auto* container = new QWidget(this);
                auto* row = new QHBoxLayout(container);
                row->setContentsMargins(0, 0, 0, 0);
                auto* edit = new QLineEdit(container);
                edit->setObjectName(QStringLiteral("motd-text"));
                auto* open = new QPushButton(tr("Редактор..."), container);
                row->addWidget(edit, 1);
                row->addWidget(open);
                connect(edit, &QLineEdit::textEdited, this, &ServerPropertiesEditor::markModified);
                connect(open, &QPushButton::clicked, this, [this, edit, serverName] {
                    // the field shows the stored form with the real section sign
                    MotdEditorDialog dialog(edit->text(), serverName, this);
                    if (dialog.exec() == QDialog::Accepted) {
                        edit->setText(dialog.motd());
                        markModified();
                    }
                });
                field = container;
                break;
            }
        }
        m_fields.insert(key, field);
        form->addRow(tr(def.label) + QLatin1Char(':'), HelpIcon::wrap(field, help, this));
    }

    // everything else found in the file
    auto* otherPage = new QWidget();
    auto* otherLayout = new QVBoxLayout(otherPage);
    auto* otherHeader = new QHBoxLayout();
    otherHeader->addWidget(new QLabel(tr("Остальные параметры (по одному в строке, ключ=значение):"), otherPage));
    otherHeader->addWidget(new HelpIcon(tr("Здесь находятся настройки из server.properties, для которых нет отдельного поля, например добавленные модами или плагинами.\n"
                                           "Порт сервера и RCON лаунчер настраивает сам, их здесь нет."),
                                        otherPage));
    otherHeader->addStretch(1);
    otherLayout->addLayout(otherHeader);
    m_other = new QPlainTextEdit(otherPage);
    QFont mono(QStringLiteral("Monospace"));
    mono.setStyleHint(QFont::Monospace);
    m_other->setFont(mono);
    connect(m_other, &QPlainTextEdit::textChanged, this, &ServerPropertiesEditor::markModified);
    otherLayout->addWidget(m_other, 1);
    tabs->addTab(otherPage, tr("Другое"));
}

bool ServerPropertiesEditor::load(const QString& path)
{
    m_path = path;
    m_loading = true;
    const bool ok = m_props.load(path);

    QStringList known;
    for (const PropDef& def : definitions()) {
        const QString key = QString::fromUtf8(def.key);
        known << key;
        const QString value = m_props.contains(key) ? m_props.value(key) : QString::fromUtf8(def.def);
        QWidget* field = m_fields.value(key);
        switch (def.kind) {
            case Kind::Bool: static_cast<QCheckBox*>(field)->setChecked(value.trimmed().toLower() == QLatin1String("true")); break;
            case Kind::Int: {
                bool good = false;
                int v = value.trimmed().toInt(&good);
                static_cast<QSpinBox*>(field)->setValue(good ? v : QString::fromUtf8(def.def).toInt());
                break;
            }
            case Kind::Choice: {
                auto* combo = static_cast<QComboBox*>(field);
                int index = combo->findData(value.trimmed());
                combo->setCurrentIndex(index >= 0 ? index : combo->findData(QString::fromUtf8(def.def)));
                break;
            }
            case Kind::Text: static_cast<QLineEdit*>(field)->setText(value); break;
            case Kind::Motd:
                field->findChild<QLineEdit*>(QStringLiteral("motd-text"))->setText(ServerProperties::decodeEscapes(value));
                break;
        }
    }

    QStringList other;
    for (const QString& key : m_props.keys()) {
        if (known.contains(key)) {
            continue;
        }
        bool managed = false;
        for (const char* m : kManagedKeys) {
            if (key == QLatin1String(m)) {
                managed = true;
            }
        }
        if (!managed) {
            other << key + QLatin1Char('=') + m_props.value(key);
        }
    }
    m_other->setPlainText(other.join('\n'));

    m_loading = false;
    m_modified = false;
    return ok;
}

bool ServerPropertiesEditor::save()
{
    // reload first, so settings changed on disk by the launcher (port, RCON) are not overwritten
    ServerProperties props;
    props.load(m_path);

    for (const PropDef& def : definitions()) {
        const QString key = QString::fromUtf8(def.key);
        QWidget* field = m_fields.value(key);
        QString value;
        switch (def.kind) {
            case Kind::Bool: value = static_cast<QCheckBox*>(field)->isChecked() ? QStringLiteral("true") : QStringLiteral("false"); break;
            case Kind::Int: value = QString::number(static_cast<QSpinBox*>(field)->value()); break;
            case Kind::Choice: value = static_cast<QComboBox*>(field)->currentData().toString(); break;
            case Kind::Text: value = static_cast<QLineEdit*>(field)->text(); break;
            case Kind::Motd:
                value = ServerProperties::encodeEscapes(field->findChild<QLineEdit*>(QStringLiteral("motd-text"))->text());
                break;
        }
        // do not add default values the file never had
        if (props.contains(key) || value != QString::fromUtf8(def.def)) {
            props.set(key, value);
        }
    }

    // free-form settings: replace the previous "other" keys with what is in the box now
    QStringList known;
    for (const PropDef& def : definitions()) {
        known << QString::fromUtf8(def.key);
    }
    for (const char* m : kManagedKeys) {
        known << QString::fromUtf8(m);
    }
    for (const QString& key : props.keys()) {
        if (!known.contains(key)) {
            props.remove(key);
        }
    }
    for (const QString& line : m_other->toPlainText().split('\n', Qt::SkipEmptyParts)) {
        const int eq = line.indexOf('=');
        if (eq <= 0) {
            continue;
        }
        const QString key = line.left(eq).trimmed();
        if (known.contains(key)) {
            continue;
        }
        props.set(key, line.mid(eq + 1));
    }

    if (!props.save(m_path)) {
        return false;
    }
    m_props = props;
    m_modified = false;
    return true;
}
