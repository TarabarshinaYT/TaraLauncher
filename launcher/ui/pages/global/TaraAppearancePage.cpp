// SPDX-License-Identifier: GPL-3.0-only
#include "TaraAppearancePage.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>

#include "Application.h"
#include "settings/SettingsObject.h"
#include "ui/TaraAppearance.h"
#include "ui/themes/ITheme.h"
#include "ui/themes/ThemeManager.h"
#include "ui/widgets/HelpIcon.h"

namespace {
QWidget* labeledSlider(const QString& label, QSlider* slider, QLabel* valueLabel, const QString& help, QWidget* parent)
{
    auto* box = new QWidget(parent);
    auto* row = new QHBoxLayout(box);
    row->setContentsMargins(0, 0, 0, 0);
    row->addWidget(new QLabel(label, box), 0);
    row->addWidget(slider, 1);
    row->addWidget(valueLabel, 0);
    row->addWidget(new HelpIcon(help, box), 0);
    return box;
}
}  // namespace

TaraAppearancePage::TaraAppearancePage(QWidget* parent) : QWidget(parent)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 6, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget();
    auto* layout = new QVBoxLayout(content);
    layout->addWidget(buildStyleGroup());
    layout->addWidget(buildBackgroundGroup());
    layout->addWidget(buildIconGroup());
    layout->addStretch(1);
    scroll->setWidget(content);
    outer->addWidget(scroll);
}

// ------------------------------------------------------------------ style

QWidget* TaraAppearancePage::buildStyleGroup()
{
    auto* group = new QGroupBox(tr("Стиль оформления"), this);
    auto* layout = new QVBoxLayout(group);

    auto* row = new QHBoxLayout();
    row->addWidget(new QLabel(tr("Стиль:"), group));
    m_theme = new QComboBox(group);
    const QString current = APPLICATION->settings()->get("ApplicationTheme").toString();
    for (ITheme* theme : APPLICATION->themeManager()->getValidApplicationThemes()) {
        m_theme->addItem(theme->name(), theme->id());
        if (!theme->tooltip().isEmpty()) {
            m_theme->setItemData(m_theme->count() - 1, theme->tooltip(), Qt::ToolTipRole);
        }
    }
    const int index = m_theme->findData(current);
    if (index >= 0) {
        m_theme->setCurrentIndex(index);
    }
    row->addWidget(m_theme, 1);
    row->addWidget(new HelpIcon(tr("Готовые оформления: «Стекло» (полупрозрачные панели), «Гладкий» (светлый, скруглённый), «Неон» (тёмный с яркими акцентами), «Минимализм» (плоский). "
                                   "Остальные стили это стандартные темы Prism. Стиль применяется сразу."),
                                group));
    layout->addLayout(row);

    m_opacity = new QSlider(Qt::Horizontal, group);
    m_opacity->setRange(0, 100);
    m_opacity->setValue(APPLICATION->settings()->get("TaraPanelOpacity").toInt());
    auto* opacityValue = new QLabel(QString::number(m_opacity->value()) + QStringLiteral("%"), group);
    layout->addWidget(labeledSlider(tr("Непрозрачность панелей:"), m_opacity, opacityValue,
                                    tr("Только для стиля «Стекло»: чем меньше значение, тем прозрачнее панели и кнопки и тем лучше видна картинка на фоне. Не помогает, если фон не включён."),
                                    group));

    connect(m_theme, &QComboBox::activated, this, [this] {
        const QString id = m_theme->currentData().toString();
        APPLICATION->settings()->set("ApplicationTheme", id);
        APPLICATION->themeManager()->applyCurrentlySelectedTheme();
    });
    connect(m_opacity, &QSlider::valueChanged, this, [opacityValue](int v) { opacityValue->setText(QString::number(v) + QStringLiteral("%")); });
    connect(m_opacity, &QSlider::sliderReleased, this, [this] {
        APPLICATION->settings()->set("TaraPanelOpacity", m_opacity->value());
        APPLICATION->themeManager()->applyCurrentlySelectedTheme();  // the stylesheet contains the alpha value
    });
    return group;
}

// ------------------------------------------------------------------ background

QWidget* TaraAppearancePage::buildBackgroundGroup()
{
    auto* s = APPLICATION->settings();
    auto* group = new QGroupBox(tr("Фон"), this);
    auto* layout = new QVBoxLayout(group);

    m_bgEnabled = new QCheckBox(tr("Показывать картинку на фоне окон"), group);
    m_bgEnabled->setChecked(s->get("TaraBackgroundEnabled").toBool());
    auto* enabledRow = new QHBoxLayout();
    enabledRow->addWidget(m_bgEnabled, 1);
    enabledRow->addWidget(new HelpIcon(tr("Картинка рисуется под всеми кнопками и списками в окнах лаунчера. С непрозрачными стилями её видно только по краям, "
                                          "а со стилем «Стекло» и малой непрозрачностью панелей она хорошо просвечивает."),
                                       group));
    layout->addLayout(enabledRow);

    auto* pathRow = new QHBoxLayout();
    m_bgPath = new QLineEdit(s->get("TaraBackgroundPath").toString(), group);
    m_bgPath->setReadOnly(true);
    m_bgPath->setPlaceholderText(tr("Картинка не выбрана"));
    auto* choose = new QPushButton(tr("Выбрать..."), group);
    auto* clear = new QPushButton(tr("Убрать"), group);
    pathRow->addWidget(m_bgPath, 1);
    pathRow->addWidget(choose);
    pathRow->addWidget(clear);
    layout->addLayout(pathRow);

    auto* modeRow = new QHBoxLayout();
    modeRow->addWidget(new QLabel(tr("Размещение:"), group));
    m_bgMode = new QComboBox(group);
    m_bgMode->addItem(tr("Заполнить (обрезать лишнее)"), "fill");
    m_bgMode->addItem(tr("Вписать целиком"), "fit");
    m_bgMode->addItem(tr("Растянуть"), "stretch");
    m_bgMode->addItem(tr("По центру"), "center");
    const int modeIndex = m_bgMode->findData(s->get("TaraBackgroundMode").toString());
    m_bgMode->setCurrentIndex(modeIndex >= 0 ? modeIndex : 0);
    modeRow->addWidget(m_bgMode, 1);
    layout->addLayout(modeRow);

    m_bgDim = new QSlider(Qt::Horizontal, group);
    m_bgDim->setRange(0, 90);
    m_bgDim->setValue(s->get("TaraBackgroundDim").toInt());
    auto* dimValue = new QLabel(QString::number(m_bgDim->value()) + QStringLiteral("%"), group);
    layout->addWidget(labeledSlider(tr("Затемнение:"), m_bgDim, dimValue, tr("Накладывает тёмную плёнку на картинку, чтобы текст на ней читался лучше."), group));

    m_bgBlur = new QSlider(Qt::Horizontal, group);
    m_bgBlur->setRange(0, 30);
    m_bgBlur->setValue(s->get("TaraBackgroundBlur").toInt());
    auto* blurValue = new QLabel(QString::number(m_bgBlur->value()), group);
    layout->addWidget(labeledSlider(tr("Размытие:"), m_bgBlur, blurValue, tr("Размывает картинку, чтобы она не отвлекала от кнопок. Ноль отключает размытие."), group));

    connect(choose, &QPushButton::clicked, this, &TaraAppearancePage::pickBackground);
    connect(clear, &QPushButton::clicked, this, [this] {
        m_bgPath->clear();
        m_bgEnabled->setChecked(false);
        saveBackgroundSettings();
    });
    connect(m_bgEnabled, &QCheckBox::toggled, this, &TaraAppearancePage::saveBackgroundSettings);
    connect(m_bgMode, &QComboBox::activated, this, &TaraAppearancePage::saveBackgroundSettings);
    connect(m_bgDim, &QSlider::valueChanged, this, [dimValue](int v) { dimValue->setText(QString::number(v) + QStringLiteral("%")); });
    connect(m_bgBlur, &QSlider::valueChanged, this, [blurValue](int v) { blurValue->setText(QString::number(v)); });
    connect(m_bgDim, &QSlider::sliderReleased, this, &TaraAppearancePage::saveBackgroundSettings);
    connect(m_bgBlur, &QSlider::sliderReleased, this, &TaraAppearancePage::saveBackgroundSettings);
    return group;
}

void TaraAppearancePage::pickBackground()
{
    const QString file = QFileDialog::getOpenFileName(this, tr("Выберите картинку для фона"), QString(), tr("Изображения (*.png *.jpg *.jpeg *.bmp *.webp *.gif)"));
    if (file.isEmpty()) {
        return;
    }
    m_bgPath->setText(file);
    m_bgEnabled->setChecked(true);
    saveBackgroundSettings();
}

void TaraAppearancePage::saveBackgroundSettings()
{
    auto* s = APPLICATION->settings();
    s->set("TaraBackgroundEnabled", m_bgEnabled->isChecked() && !m_bgPath->text().isEmpty());
    s->set("TaraBackgroundPath", m_bgPath->text());
    s->set("TaraBackgroundMode", m_bgMode->currentData().toString());
    s->set("TaraBackgroundDim", m_bgDim->value());
    s->set("TaraBackgroundBlur", m_bgBlur->value());
    TaraAppearance::reloadBackground();
}

// ------------------------------------------------------------------ icon

QWidget* TaraAppearancePage::buildIconGroup()
{
    auto* s = APPLICATION->settings();
    auto* group = new QGroupBox(tr("Иконка приложения"), this);
    auto* layout = new QVBoxLayout(group);

    auto* header = new QHBoxLayout();
    header->addWidget(new QLabel(tr("Выберите иконку из набора или загрузите свою:"), group), 1);
    header->addWidget(new HelpIcon(tr("Иконка меняется у окна лаунчера и на панели задач (рядом с кнопкой «Пуск»), пока лаунчер запущен. "
                                      "Файл программы (.exe) нельзя поменять во время работы, поэтому для проводника есть кнопка обновления ярлыка на рабочем столе ниже."),
                                   group));
    layout->addLayout(header);

    m_presets = new QButtonGroup(this);
    m_presets->setExclusive(true);
    auto* grid = new QGridLayout();
    int column = 0;
    const QString selected = s->get("TaraIconPreset").toString();
    for (const QString& id : TaraAppearance::presetIcons()) {
        auto* button = new QToolButton(group);
        button->setCheckable(true);
        button->setIcon(QIcon(TaraAppearance::presetIconPixmap(id, 64)));
        button->setIconSize(QSize(56, 56));
        button->setToolTip(TaraAppearance::presetIconName(id));
        button->setAutoRaise(false);
        button->setChecked(selected == id || (selected.isEmpty() && id == QLatin1String("tara")));
        m_presets->addButton(button);
        grid->addWidget(button, 0, column++);
        connect(button, &QToolButton::clicked, this, [this, id] { selectPreset(id); });
    }
    grid->setColumnStretch(column, 1);
    layout->addLayout(grid);

    auto* customRow = new QHBoxLayout();
    auto* custom = new QPushButton(tr("Своя иконка..."), group);
    customRow->addWidget(custom);
    customRow->addStretch(1);
    layout->addLayout(customRow);

    auto* shortcutRow = new QHBoxLayout();
    auto* shortcutButton = new QPushButton(tr("Применить к ярлыку на рабочем столе"), group);
    shortcutRow->addWidget(shortcutButton);
    shortcutRow->addWidget(new HelpIcon(tr("Ярлык на рабочем столе обновится сразу: лаунчер удалит свой старый ярлык и создаст новый с выбранной иконкой. Чужие файлы на рабочем столе не трогаются.\n\n"
                                           "Примечание. Если лаунчер закреплён на панели задач, Windows не позволяет программам менять такие ярлыки: пока лаунчер запущен, на панели будет новая иконка, "
                                           "а чтобы она осталась и после закрытия, один раз открепите лаунчер и закрепите заново."),
                                        group));
    shortcutRow->addStretch(1);
    layout->addLayout(shortcutRow);
    m_shortcutStatus = new QLabel(group);
    m_shortcutStatus->setWordWrap(true);
    layout->addWidget(m_shortcutStatus);

    connect(custom, &QPushButton::clicked, this, &TaraAppearancePage::pickIcon);
    connect(shortcutButton, &QPushButton::clicked, this, [this] {
        QString error;
        if (TaraAppearance::refreshDesktopShortcut(&error)) {
            m_shortcutStatus->setText(tr("Готово: ярлык на рабочем столе обновлён."));
        } else {
            m_shortcutStatus->setText(error);
        }
    });
    return group;
}

void TaraAppearancePage::selectPreset(const QString& id)
{
    APPLICATION->settings()->set("TaraIconPreset", id);
    TaraAppearance::applyIconToWindows();
}

void TaraAppearancePage::pickIcon()
{
    const QString file = QFileDialog::getOpenFileName(this, tr("Выберите иконку"), QString(), tr("Иконки и изображения (*.png *.ico *.jpg *.jpeg *.bmp)"));
    if (file.isEmpty()) {
        return;
    }
    QPixmap test(file);
    if (test.isNull()) {
        m_shortcutStatus->setText(tr("Не удалось открыть выбранный файл как изображение."));
        return;
    }
    auto* s = APPLICATION->settings();
    s->set("TaraIconCustomPath", file);
    s->set("TaraIconPreset", "custom");
    // a custom icon is not one of the preset buttons
    if (auto* checked = m_presets->checkedButton()) {
        m_presets->setExclusive(false);
        checked->setChecked(false);
        m_presets->setExclusive(true);
    }
    TaraAppearance::applyIconToWindows();
}
