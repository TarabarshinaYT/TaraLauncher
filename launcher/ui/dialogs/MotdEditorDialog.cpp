// SPDX-License-Identifier: GPL-3.0-only
#include "MotdEditorDialog.h"

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextCursor>
#include <QVBoxLayout>

#include "ui/widgets/HelpIcon.h"
#include "ui/widgets/MotdPreview.h"

namespace {
struct ColorButton {
    const char* code;
    const char* name;
};
const ColorButton kColors[] = {
    { "0", "Чёрный" },        { "1", "Тёмно-синий" },  { "2", "Тёмно-зелёный" }, { "3", "Тёмно-бирюзовый" },
    { "4", "Тёмно-красный" }, { "5", "Фиолетовый" },   { "6", "Золотой" },       { "7", "Серый" },
    { "8", "Тёмно-серый" },   { "9", "Синий" },        { "a", "Зелёный" },       { "b", "Голубой" },
    { "c", "Красный" },       { "d", "Розовый" },      { "e", "Жёлтый" },        { "f", "Белый" },
};
struct FormatButton {
    const char* code;
    const char* label;
    const char* name;
};
const FormatButton kFormats[] = {
    { "l", "Ж", "Жирный" },  { "o", "К", "Курсив" },        { "n", "Ч", "Подчёркнутый" },
    { "m", "З", "Зачёркнутый" }, { "k", "?", "Случайные символы (мерцающий текст)" }, { "r", "Сброс", "Сбросить цвет и форматирование" },
};
}  // namespace

MotdEditorDialog::MotdEditorDialog(const QString& storedMotd, const QString& serverName, QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Редактор описания сервера (MOTD)"));
    resize(640, 460);

    auto* layout = new QVBoxLayout(this);

    auto* header = new QHBoxLayout();
    header->addWidget(new QLabel(tr("Текст описания (до двух строк):"), this));
    header->addWidget(new HelpIcon(tr("Это текст под названием сервера в списке серверов Minecraft.\n"
                                      "Выделите часть текста и нажмите цвет или стиль, чтобы применить его только к ней.\n"
                                      "Цвета и стили записываются кодами вида &a, их можно набирать вручную.\n"
                                      "Вторая строка появится после нажатия Enter. Больше двух строк игра не показывает."),
                                   this));
    header->addStretch(1);
    layout->addLayout(header);

    m_edit = new QPlainTextEdit(this);
    m_edit->setMaximumHeight(72);
    QFont mono(QStringLiteral("Monospace"));
    mono.setStyleHint(QFont::Monospace);
    m_edit->setFont(mono);
    m_edit->setPlainText(Motd::sectionToAmpersand(storedMotd));
    layout->addWidget(m_edit);

    // colours
    layout->addWidget(new QLabel(tr("Цвет:"), this));
    auto* colors = new QGridLayout();
    colors->setSpacing(4);
    for (int i = 0; i < 16; ++i) {
        auto* button = new QPushButton(this);
        button->setFixedSize(28, 28);
        const QColor c = Motd::colorForCode(QChar(kColors[i].code[0]));
        button->setStyleSheet(QStringLiteral("QPushButton { background: %1; border: 1px solid #555; border-radius: 3px; }"
                                             "QPushButton:hover { border: 2px solid white; }")
                                  .arg(c.name()));
        button->setToolTip(tr(kColors[i].name) + QStringLiteral("  (&") + QLatin1String(kColors[i].code) + QLatin1Char(')'));
        const QString code = QStringLiteral("&") + QLatin1String(kColors[i].code);
        connect(button, &QPushButton::clicked, this, [this, code] { insertCode(code); });
        colors->addWidget(button, i / 8, i % 8);
    }
    colors->setColumnStretch(8, 1);
    layout->addLayout(colors);

    // styles
    layout->addWidget(new QLabel(tr("Стиль:"), this));
    auto* formats = new QHBoxLayout();
    for (const auto& f : kFormats) {
        auto* button = new QPushButton(QString::fromUtf8(f.label), this);
        button->setToolTip(tr(f.name) + QStringLiteral("  (&") + QLatin1String(f.code) + QLatin1Char(')'));
        const QString code = QStringLiteral("&") + QLatin1String(f.code);
        connect(button, &QPushButton::clicked, this, [this, code] { insertCode(code); });
        formats->addWidget(button);
    }
    formats->addStretch(1);
    layout->addLayout(formats);

    layout->addWidget(new QLabel(tr("Предпросмотр:"), this));
    m_preview = new MotdPreview(this);
    m_preview->setServerName(serverName);
    layout->addWidget(m_preview);
    layout->addStretch(1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Применить"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Отмена"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(m_edit, &QPlainTextEdit::textChanged, this, [this] {
        limitLines();
        m_preview->setMotd(m_edit->toPlainText());
    });
    m_preview->setMotd(m_edit->toPlainText());
}

void MotdEditorDialog::limitLines()
{
    // the server list shows two lines only; drop anything after the second line break
    const QString text = m_edit->toPlainText();
    const int first = text.indexOf('\n');
    if (first < 0) {
        return;
    }
    const int second = text.indexOf('\n', first + 1);
    if (second < 0) {
        return;
    }
    const QSignalBlocker blocker(m_edit);
    m_edit->setPlainText(text.left(second));
    m_edit->moveCursor(QTextCursor::End);
}

void MotdEditorDialog::insertCode(const QString& code)
{
    QTextCursor cursor = m_edit->textCursor();
    if (cursor.hasSelection()) {
        const int start = cursor.selectionStart();
        const int end = cursor.selectionEnd();
        cursor.beginEditBlock();
        cursor.setPosition(end);
        cursor.insertText(QStringLiteral("&r"));
        cursor.setPosition(start);
        cursor.insertText(code);
        cursor.endEditBlock();
    } else {
        cursor.insertText(code);
    }
    m_edit->setTextCursor(cursor);
    m_edit->setFocus();
}

QString MotdEditorDialog::motd() const
{
    return Motd::ampersandToSection(m_edit->toPlainText());
}
