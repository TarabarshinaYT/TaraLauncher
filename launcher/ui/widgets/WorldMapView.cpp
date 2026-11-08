// SPDX-License-Identifier: GPL-3.0-only
#include "WorldMapView.h"

#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <cmath>

namespace {
constexpr double kMinScale = 0.5;
constexpr double kMaxScale = 48;
}  // namespace

WorldMapView::WorldMapView(QWidget* parent) : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumSize(360, 300);
    setCursor(Qt::OpenHandCursor);
}

void WorldMapView::setPlayers(const QList<WorldData::PlayerMarker>& players)
{
    m_players = players;
    update();
}

void WorldMapView::setSpawn(const QPointF& block, bool valid)
{
    m_spawn = block;
    m_hasSpawn = valid;
    update();
}

void WorldMapView::centerOnBlock(const QPointF& block)
{
    m_center = QPointF(block.x() / 16.0, block.y() / 16.0);
    update();
}

void WorldMapView::fitWorld()
{
    if (!m_data || m_data->chunkCount() == 0) {
        m_center = QPointF(0, 0);
        m_scale = 4;
        update();
        return;
    }
    const QRect b = m_data->chunkBounds();
    m_center = QPointF(b.x() + b.width() / 2.0, b.y() + b.height() / 2.0);
    const double sx = width() / double(b.width() + 4);
    const double sy = height() / double(b.height() + 4);
    m_scale = qBound(kMinScale, qMin(sx, sy), kMaxScale);
    update();
}

QPointF WorldMapView::screenToChunk(const QPointF& p) const
{
    return QPointF((p.x() - width() / 2.0) / m_scale + m_center.x(), (p.y() - height() / 2.0) / m_scale + m_center.y());
}

QPointF WorldMapView::chunkToScreen(const QPointF& chunk) const
{
    return QPointF((chunk.x() - m_center.x()) * m_scale + width() / 2.0, (chunk.y() - m_center.y()) * m_scale + height() / 2.0);
}

void WorldMapView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0x14, 0x18, 0x1F));

    if (m_data && m_data->chunkCount() > 0) {
        // one image pixel per visible chunk, then scaled up without smoothing: fast at any zoom
        const QPointF topLeft = screenToChunk(QPointF(0, 0));
        const QPointF bottomRight = screenToChunk(QPointF(width(), height()));
        const int x0 = static_cast<int>(std::floor(topLeft.x()));
        const int z0 = static_cast<int>(std::floor(topLeft.y()));
        const int x1 = static_cast<int>(std::ceil(bottomRight.x()));
        const int z1 = static_cast<int>(std::ceil(bottomRight.y()));
        const int w = x1 - x0 + 1;
        const int h = z1 - z0 + 1;
        if (w > 0 && h > 0 && w < 6000 && h < 6000) {
            QImage image(w, h, QImage::Format_ARGB32);
            image.fill(Qt::transparent);
            const quint32 oldest = m_data->oldestTime();
            const quint32 span = qMax<quint32>(1, m_data->newestTime() - oldest);
            for (int z = 0; z < h; ++z) {
                auto* line = reinterpret_cast<QRgb*>(image.scanLine(z));
                for (int x = 0; x < w; ++x) {
                    const quint32 t = m_data->chunkTime(x0 + x, z0 + z);
                    if (t == 0) {
                        continue;
                    }
                    // recently changed chunks are brighter, long untouched ones darker
                    const double age = (t > oldest ? t - oldest : 0) / double(span);
                    const int shade = 90 + static_cast<int>(age * 100);
                    line[x] = qRgb(shade / 3, shade, shade / 2);
                }
            }
            const QPointF target = chunkToScreen(QPointF(x0, z0));
            p.setRenderHint(QPainter::SmoothPixmapTransform, false);
            p.drawImage(QRectF(target, QSizeF(w * m_scale, h * m_scale)), image);
        }
    } else {
        p.setPen(QColor(0x90, 0x98, 0xA4));
        p.drawText(rect(), Qt::AlignCenter, tr("В этом измерении пока нет сгенерированных чанков.\nЗапустите сервер и зайдите в мир."));
    }

    // grid: region borders (32 chunks) from a small zoom, chunk borders when zoomed in
    p.setRenderHint(QPainter::Antialiasing, false);
    const QPointF tl = screenToChunk(QPointF(0, 0));
    const QPointF br = screenToChunk(QPointF(width(), height()));
    if (m_scale >= 8) {
        p.setPen(QColor(255, 255, 255, 22));
        for (int cx = static_cast<int>(std::floor(tl.x())); cx <= std::ceil(br.x()); ++cx) {
            const double sx = chunkToScreen(QPointF(cx, 0)).x();
            p.drawLine(QPointF(sx, 0), QPointF(sx, height()));
        }
        for (int cz = static_cast<int>(std::floor(tl.y())); cz <= std::ceil(br.y()); ++cz) {
            const double sy = chunkToScreen(QPointF(0, cz)).y();
            p.drawLine(QPointF(0, sy), QPointF(width(), sy));
        }
    }
    if (m_scale >= 1) {
        p.setPen(QColor(255, 255, 255, 45));
        for (int rx = static_cast<int>(std::floor(tl.x() / 32)); rx <= std::ceil(br.x() / 32); ++rx) {
            const double sx = chunkToScreen(QPointF(rx * 32, 0)).x();
            p.drawLine(QPointF(sx, 0), QPointF(sx, height()));
        }
        for (int rz = static_cast<int>(std::floor(tl.y() / 32)); rz <= std::ceil(br.y() / 32); ++rz) {
            const double sy = chunkToScreen(QPointF(0, rz * 32)).y();
            p.drawLine(QPointF(0, sy), QPointF(width(), sy));
        }
    }
    // axes through 0,0
    p.setPen(QColor(0xFF, 0x88, 0x55, 130));
    const QPointF origin = chunkToScreen(QPointF(0, 0));
    p.drawLine(QPointF(origin.x(), 0), QPointF(origin.x(), height()));
    p.drawLine(QPointF(0, origin.y()), QPointF(width(), origin.y()));

    p.setRenderHint(QPainter::Antialiasing, true);

    // world spawn
    if (m_hasSpawn) {
        const QPointF s = chunkToScreen(QPointF(m_spawn.x() / 16.0, m_spawn.y() / 16.0));
        p.setPen(QPen(QColor(0xFF, 0xD7, 0x4A), 2));
        p.drawLine(s + QPointF(-6, 0), s + QPointF(6, 0));
        p.drawLine(s + QPointF(0, -6), s + QPointF(0, 6));
        p.setPen(QColor(0xFF, 0xD7, 0x4A));
        p.drawText(s + QPointF(8, -6), tr("Спавн"));
    }

    // players
    for (const auto& player : m_players) {
        const QPointF s = chunkToScreen(QPointF(player.block.x() / 16.0, player.block.y() / 16.0));
        const QColor color = player.online ? QColor(0x4C, 0xE0, 0x6A) : QColor(0xB0, 0xB6, 0xC0);
        p.setPen(QPen(Qt::black, 1));
        p.setBrush(color);
        p.drawEllipse(s, 6, 6);
        const QString label = player.online ? player.name : tr("%1 (не в сети)").arg(player.name);
        const QRectF box(s.x() + 9, s.y() - 10, p.fontMetrics().horizontalAdvance(label) + 8, 18);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 160));
        p.drawRoundedRect(box, 3, 3);
        p.setPen(Qt::white);
        p.drawText(box, Qt::AlignCenter, label);
    }

    // scale bar (in blocks)
    const double blocksPerPixel = 16.0 / m_scale;
    const int barBlocks = (blocksPerPixel * 100 > 1000) ? 1000 : (blocksPerPixel * 100 > 100 ? 100 : 16);
    const double barPixels = barBlocks / blocksPerPixel;
    p.setPen(QPen(Qt::white, 2));
    p.drawLine(QPointF(12, height() - 14), QPointF(12 + barPixels, height() - 14));
    p.setPen(Qt::white);
    p.drawText(QPointF(14 + barPixels, height() - 10), tr("%1 блоков").arg(barBlocks));
}

void WorldMapView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_lastMouse = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}

void WorldMapView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging) {
        const QPoint delta = event->pos() - m_lastMouse;
        m_lastMouse = event->pos();
        m_center -= QPointF(delta.x() / m_scale, delta.y() / m_scale);
        update();
    }
    const QPointF chunk = screenToChunk(event->position());
    const bool explored = m_data && m_data->hasChunk(static_cast<int>(std::floor(chunk.x())), static_cast<int>(std::floor(chunk.y())));
    emit hoverBlock(static_cast<int>(std::floor(chunk.x() * 16)), static_cast<int>(std::floor(chunk.y() * 16)), explored);
}

void WorldMapView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        setCursor(Qt::OpenHandCursor);
    }
}

void WorldMapView::wheelEvent(QWheelEvent* event)
{
    const double steps = event->angleDelta().y() / 120.0;
    if (steps == 0) {
        return;
    }
    // keep the point under the cursor fixed while zooming
    const QPointF before = screenToChunk(event->position());
    m_scale = qBound(kMinScale, m_scale * std::pow(1.25, steps), kMaxScale);
    const QPointF after = screenToChunk(event->position());
    m_center += before - after;
    update();
}

void WorldMapView::leaveEvent(QEvent*)
{
    emit hoverLeft();
}
