// SPDX-License-Identifier: GPL-3.0-only
#include "PlayersTab.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QTableWidget>
#include <QVBoxLayout>

#include "servers/ServerProcess.h"
#include "ui/widgets/HelpIcon.h"

namespace {
QString when(const QDateTime& t)
{
    return t.isValid() ? QLocale().toString(t, QLocale::ShortFormat) : QStringLiteral("—");
}

QTableWidget* makeTable(const QStringList& headers, QWidget* parent)
{
    auto* table = new QTableWidget(0, headers.size(), parent);
    table->setHorizontalHeaderLabels(headers);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->verticalHeader()->hide();
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    return table;
}
}  // namespace

PlayersTab::PlayersTab(ServerProcess* server, QWidget* parent) : QWidget(parent), m_server(server)
{
    auto* layout = new QVBoxLayout(this);

    auto* header = new QHBoxLayout();
    header->addWidget(new QLabel(tr("Игроки"), this));
    header->addWidget(new HelpIcon(tr("История строится по журналу сервера: лаунчер замечает, когда игрок зашёл и вышел.\n"
                                      "Входы и выходы, случившиеся, пока лаунчер был закрыт, точно учесть нельзя: после запуска лаунчер только проверяет, кто сейчас на сервере.\n"
                                      "Время игры считается по сессиям: от входа до выхода или до остановки сервера."),
                                   this));
    header->addStretch(1);
    layout->addLayout(header);

    m_players = makeTable({ tr("Игрок"), tr("Сейчас"), tr("Первый вход"), tr("Последний раз"), tr("Сессий"), tr("Всего времени") }, this);
    layout->addWidget(m_players, 3);

    layout->addWidget(new QLabel(tr("Последние сессии"), this));
    m_sessions = makeTable({ tr("Игрок"), tr("Вход"), tr("Выход"), tr("Длительность") }, this);
    layout->addWidget(m_sessions, 2);

    m_timer.setInterval(3000);
    connect(&m_timer, &QTimer::timeout, this, &PlayersTab::refresh);
    m_timer.start();
    refresh();
}

void PlayersTab::refresh()
{
    const PlayerHistory& history = m_server->history();
    const QDateTime now = QDateTime::currentDateTime();

    const auto players = history.players();
    m_players->setRowCount(players.size());
    for (int row = 0; row < players.size(); ++row) {
        const auto& p = players[row];
        qint64 total = p.totalSeconds;
        if (p.online && p.currentJoin.isValid()) {
            total += qMax<qint64>(0, p.currentJoin.secsTo(now));  // the running session counts too
        }
        const QStringList cells = { p.name,
                                    p.online ? tr("в игре") : QString(),
                                    when(p.firstSeen),
                                    p.online ? tr("сейчас") : when(p.lastSeen),
                                    QString::number(p.sessions),
                                    PlayerHistory::formatDuration(total) };
        for (int col = 0; col < cells.size(); ++col) {
            m_players->setItem(row, col, new QTableWidgetItem(cells[col]));
        }
    }

    const auto sessions = history.recentSessions(100);
    m_sessions->setRowCount(sessions.size());
    for (int row = 0; row < sessions.size(); ++row) {
        const auto& s = sessions[row];
        const QStringList cells = { s.name, when(s.join), when(s.leave), PlayerHistory::formatDuration(qMax<qint64>(0, s.join.secsTo(s.leave))) };
        for (int col = 0; col < cells.size(); ++col) {
            m_sessions->setItem(row, col, new QTableWidgetItem(cells[col]));
        }
    }
}
