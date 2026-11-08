// SPDX-License-Identifier: GPL-3.0-only
#include "MapTab.h"

#include <QComboBox>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>

#include "servers/ServerProcess.h"
#include "servers/ServerProperties.h"
#include "ui/widgets/HelpIcon.h"
#include "ui/widgets/WorldMapView.h"

MapTab::MapTab(ServerProcess* server, QWidget* parent) : QWidget(parent), m_server(server)
{
    auto* layout = new QVBoxLayout(this);

    auto* bar = new QHBoxLayout();
    bar->addWidget(new QLabel(tr("Измерение:"), this));
    m_dimension = new QComboBox(this);
    m_dimension->addItem(tr("Верхний мир"), static_cast<int>(WorldData::Dimension::Overworld));
    m_dimension->addItem(tr("Нижний мир"), static_cast<int>(WorldData::Dimension::Nether));
    m_dimension->addItem(tr("Энд"), static_cast<int>(WorldData::Dimension::End));
    bar->addWidget(m_dimension);

    auto* spawn = new QPushButton(tr("К спавну"), this);
    auto* all = new QPushButton(tr("Весь мир"), this);
    auto* refresh = new QPushButton(tr("Обновить"), this);
    auto* save = new QPushButton(tr("Сохранить мир"), this);
    bar->addWidget(spawn);
    bar->addWidget(all);
    bar->addWidget(refresh);
    bar->addWidget(HelpIcon::wrap(save,
                                  tr("Просит работающий сервер сразу записать мир на диск (команда save-all flush). Карта читает файлы мира, а сервер записывает новые области не мгновенно, "
                                     "поэтому только что исследованные места могут появиться на карте с задержкой. После этой кнопки нажмите «Обновить»."),
                                  this));
    bar->addStretch(1);
    bar->addWidget(new HelpIcon(tr("Карта показывает, какие области мира уже созданы (каждый квадрат это чанк 16×16 блоков). Яркие квадраты изменялись недавно, тёмные давно.\n"
                                   "Мышью можно двигать карту, колесом менять масштаб.\n"
                                   "Зелёная точка это игрок в сети (его положение берётся у работающего сервера), серая это игрок, который был в сети раньше: показывается место, где он вышел.\n"
                                   "Жёлтый крестик это точка возрождения мира. Рисунка местности (деревьев, воды) пока нет."),
                                this));
    layout->addLayout(bar);

    m_view = new WorldMapView(this);
    m_view->setData(&m_data);
    layout->addWidget(m_view, 1);

    auto* info = new QHBoxLayout();
    m_status = new QLabel(this);
    m_hover = new QLabel(this);
    info->addWidget(m_status, 1);
    info->addWidget(m_hover);
    layout->addLayout(info);

    connect(m_dimension, &QComboBox::currentIndexChanged, this, [this] { reload(true); });
    connect(refresh, &QPushButton::clicked, this, [this] { reload(false); });
    connect(all, &QPushButton::clicked, this, [this] { m_view->fitWorld(); });
    connect(spawn, &QPushButton::clicked, this, [this] {
        QPointF block;
        if (WorldData::readSpawn(m_server->directory(), levelName(), &block)) {
            m_view->centerOnBlock(block);
        } else {
            m_view->centerOnBlock(QPointF(0, 0));
        }
    });
    connect(save, &QPushButton::clicked, this, [this] {
        QString reply;
        if (!m_server->query(QStringLiteral("save-all flush"), &reply)) {
            m_status->setText(tr("Сервер не запущен, мир уже сохранён на диске."));
            return;
        }
        reload(false);
    });
    connect(m_view, &WorldMapView::hoverBlock, this, [this](int x, int z, bool explored) {
        m_hover->setText(tr("X: %1  Z: %2  %3").arg(x).arg(z).arg(explored ? tr("(создано)") : tr("(пусто)")));
    });
    connect(m_view, &WorldMapView::hoverLeft, this, [this] { m_hover->clear(); });

    m_timer.setInterval(5000);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        refreshLivePlayers();
        static int ticks = 0;
        if (++ticks % 3 == 0) {
            reload(false);  // pick up newly saved chunks every ~15 seconds
        }
    });
    m_timer.start();

    reload(true);
}

QString MapTab::levelName() const
{
    ServerProperties props;
    props.load(QDir(m_server->directory()).filePath(QStringLiteral("server.properties")));
    return props.value(QStringLiteral("level-name"), QStringLiteral("world")).trimmed();
}

WorldData::Dimension MapTab::dimension() const
{
    return static_cast<WorldData::Dimension>(m_dimension->currentData().toInt());
}

void MapTab::reload(bool refit)
{
    const QString level = levelName();
    const auto dim = dimension();
    const int chunks = m_data.loadRegions(WorldData::regionDirCandidates(m_server->directory(), level, dim));
    m_filePlayers = WorldData::readPlayers(m_server->directory(), level, dim);

    QPointF spawn;
    const bool hasSpawn = dim == WorldData::Dimension::Overworld && WorldData::readSpawn(m_server->directory(), level, &spawn);
    m_view->setSpawn(spawn, hasSpawn);

    refreshLivePlayers();

    if (refit || m_firstLoad) {
        m_view->fitWorld();
        m_firstLoad = false;
    }
    const long long areaBlocks = static_cast<long long>(chunks) * 256;
    m_status->setText(chunks > 0 ? tr("Создано чанков: %1 (около %2 блоков²)").arg(chunks).arg(areaBlocks) : tr("Мир ещё не создан или в этом измерении пусто."));
    m_view->update();
}

void MapTab::refreshLivePlayers()
{
    QList<WorldData::PlayerMarker> markers = m_filePlayers;
    if (m_server->status() == ServerProcess::Status::Running) {
        static const QRegularExpression number(QStringLiteral("(-?\\d+(?:\\.\\d+)?)d"));
        for (const QString& name : m_server->history().onlineNames()) {
            QString reply;
            if (!m_server->query(QStringLiteral("data get entity %1 Pos").arg(name), &reply)) {
                continue;
            }
            QList<double> values;
            auto it = number.globalMatch(reply.mid(reply.indexOf('[')));
            while (it.hasNext()) {
                values << it.next().captured(1).toDouble();
            }
            if (values.size() < 3) {
                continue;
            }
            // which dimension is the player in?
            WorldData::Dimension dim = WorldData::Dimension::Overworld;
            QString dimReply;
            if (m_server->query(QStringLiteral("data get entity %1 Dimension").arg(name), &dimReply)) {
                if (dimReply.contains(QLatin1String("nether"))) {
                    dim = WorldData::Dimension::Nether;
                } else if (dimReply.contains(QLatin1String("the_end"))) {
                    dim = WorldData::Dimension::End;
                }
            }
            if (dim != dimension()) {
                // online, but in another dimension: do not show a stale position from the file either
                for (int i = markers.size() - 1; i >= 0; --i) {
                    if (markers[i].name.compare(name, Qt::CaseInsensitive) == 0) {
                        markers.removeAt(i);
                    }
                }
                continue;
            }
            bool replaced = false;
            for (auto& marker : markers) {
                if (marker.name.compare(name, Qt::CaseInsensitive) == 0) {
                    marker.block = QPointF(values[0], values[2]);
                    marker.online = true;
                    replaced = true;
                }
            }
            if (!replaced) {
                WorldData::PlayerMarker marker;
                marker.name = name;
                marker.block = QPointF(values[0], values[2]);
                marker.online = true;
                marker.dimension = dim;
                markers << marker;
            }
        }
    }
    m_view->setPlayers(markers);
}
