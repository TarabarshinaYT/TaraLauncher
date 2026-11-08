// SPDX-License-Identifier: GPL-3.0-only
#include "TaraAppearance.h"

#include <QApplication>
#include <QBuffer>
#include <QDataStream>
#include <QSaveFile>
#include <QDialog>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFontMetrics>
#include <QImage>
#include <QLinearGradient>
#include <QMainWindow>
#include <QPainter>
#include <QPainterPath>
#include <QStandardPaths>
#include <QWidget>

#include "Application.h"
#include "settings/SettingsObject.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#endif

namespace {

// ------------------------------------------------------------------ background

class BackgroundPainter : public QObject {
   public:
    explicit BackgroundPainter(QObject* parent) : QObject(parent) { reload(); }

    void reload()
    {
        auto* s = APPLICATION->settings();
        m_enabled = s->get("TaraBackgroundEnabled").toBool();
        m_dim = qBound(0, s->get("TaraBackgroundDim").toInt(), 90);
        m_blur = qBound(0, s->get("TaraBackgroundBlur").toInt(), 30);
        m_mode = s->get("TaraBackgroundMode").toString();
        const QString path = s->get("TaraBackgroundPath").toString();
        m_source = QPixmap();
        if (m_enabled && !path.isEmpty()) {
            m_source.load(path);
        }
        m_cache = QPixmap();
        m_cacheSize = QSize();
    }

    bool active() const { return m_enabled && !m_source.isNull(); }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::Show && !g_icon.isNull()) {
            // the main window sets its own icon in its constructor, so the chosen one is applied when it is shown
            auto* shown = qobject_cast<QWidget*>(watched);
            if (shown && shown->isWindow() &&
                (qobject_cast<QMainWindow*>(shown) || qobject_cast<QDialog*>(shown) || shown->property("taraBackground").toBool())) {
                shown->setWindowIcon(g_icon);
            }
            return false;
        }
        if (event->type() != QEvent::Paint || !active()) {
            return false;
        }
        auto* widget = qobject_cast<QWidget*>(watched);
        if (!widget || !widget->isWindow() || widget->size().isEmpty()) {
            return false;
        }
        // only real windows: main window, dialogs and our own windows (popups and tooltips are left alone)
        if (!qobject_cast<QMainWindow*>(widget) && !qobject_cast<QDialog*>(widget) && !widget->property("taraBackground").toBool()) {
            return false;
        }
        QPainter painter(widget);
        painter.drawPixmap(0, 0, render(widget->size()));
        return false;  // the widget still paints its own content on top
    }

   private:
    QPixmap render(const QSize& size)
    {
        if (m_cacheSize == size && !m_cache.isNull()) {
            return m_cache;
        }
        QPixmap out(size);
        out.fill(QColor(10, 12, 18));
        QPainter p(&out);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        if (m_mode == QLatin1String("stretch")) {
            p.drawPixmap(QRect(QPoint(0, 0), size), m_source);
        } else if (m_mode == QLatin1String("center")) {
            p.drawPixmap((size.width() - m_source.width()) / 2, (size.height() - m_source.height()) / 2, m_source);
        } else if (m_mode == QLatin1String("fit")) {
            const QPixmap scaled = m_source.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            p.drawPixmap((size.width() - scaled.width()) / 2, (size.height() - scaled.height()) / 2, scaled);
        } else {  // fill: cover the whole window, crop the overflow
            const QPixmap scaled = m_source.scaled(size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            p.drawPixmap((size.width() - scaled.width()) / 2, (size.height() - scaled.height()) / 2, scaled);
        }
        p.end();

        if (m_blur > 0) {
            // cheap blur: shrink and enlarge with smoothing
            const int divisor = 1 + m_blur / 2;
            const QSize small(qMax(1, size.width() / divisor), qMax(1, size.height() / divisor));
            out = out.scaled(small, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
        if (m_dim > 0) {
            QPainter dim(&out);
            dim.fillRect(out.rect(), QColor(0, 0, 0, m_dim * 255 / 100));
        }
        m_cache = out;
        m_cacheSize = size;
        return m_cache;
    }

    bool m_enabled = false;
    int m_dim = 35;
    int m_blur = 0;
    QString m_mode = QStringLiteral("fill");
    QPixmap m_source;
    QPixmap m_cache;
    QSize m_cacheSize;
};

BackgroundPainter* g_background = nullptr;
QIcon g_icon;  // the selected application icon, applied to every window that gets shown

// ------------------------------------------------------------------ icon presets

struct Preset {
    const char* id;
    const char* name;
    QRgb top;
    QRgb bottom;
};

const Preset kPresets[] = {
    { "tara", "Tara (синий)", qRgb(0x4C, 0x7B, 0xF4), qRgb(0x8A, 0x4C, 0xF4) },
    { "ember", "Уголь (оранжевый)", qRgb(0xFF, 0xA0, 0x3A), qRgb(0xE0, 0x30, 0x30) },
    { "forest", "Лес (зелёный)", qRgb(0x5C, 0xD0, 0x6A), qRgb(0x1C, 0x7C, 0x5A) },
    { "ocean", "Океан (бирюзовый)", qRgb(0x3A, 0xD0, 0xE0), qRgb(0x2A, 0x5C, 0xC8) },
    { "night", "Ночь (тёмный)", qRgb(0x22, 0x28, 0x44), qRgb(0x0A, 0x0C, 0x18) },
    { "sakura", "Сакура (розовый)", qRgb(0xFF, 0x9C, 0xC8), qRgb(0xC8, 0x4C, 0xA0) },
};

const Preset* findPreset(const QString& id)
{
    for (const Preset& p : kPresets) {
        if (id == QLatin1String(p.id)) {
            return &p;
        }
    }
    return &kPresets[0];
}

}  // namespace

namespace TaraAppearance {

void install()
{
    if (!g_background) {
        g_background = new BackgroundPainter(qApp);
        qApp->installEventFilter(g_background);
    }
    applyIconToWindows();
}

void reloadBackground()
{
    if (!g_background) {
        return;
    }
    g_background->reload();
    for (QWidget* w : QApplication::topLevelWidgets()) {
        w->update();
    }
}

QStringList presetIcons()
{
    QStringList out;
    for (const Preset& p : kPresets) {
        out << QString::fromLatin1(p.id);
    }
    return out;
}

QString presetIconName(const QString& id)
{
    return QObject::tr(findPreset(id)->name);
}

QPixmap presetIconPixmap(const QString& id, int size)
{
    const Preset* preset = findPreset(id);
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    QLinearGradient gradient(0, 0, size, size);
    gradient.setColorAt(0, QColor(preset->top));
    gradient.setColorAt(1, QColor(preset->bottom));
    QPainterPath shape;
    shape.addRoundedRect(QRectF(size * 0.04, size * 0.04, size * 0.92, size * 0.92), size * 0.22, size * 0.22);
    p.fillPath(shape, gradient);

    // soft highlight on the upper half
    p.setClipPath(shape);
    p.fillRect(QRectF(0, 0, size, size * 0.5), QColor(255, 255, 255, 36));
    p.setClipping(false);

    // white letter T
    QFont font;
    font.setBold(true);
    font.setPixelSize(static_cast<int>(size * 0.62));
    p.setFont(font);
    p.setPen(Qt::white);
    p.drawText(QRectF(0, 0, size, size), Qt::AlignCenter, QStringLiteral("T"));
    return pixmap;
}

QIcon currentIcon()
{
    auto* s = APPLICATION->settings();
    const QString preset = s->get("TaraIconPreset").toString();
    if (preset == QLatin1String("custom")) {
        const QString path = s->get("TaraIconCustomPath").toString();
        QIcon icon(path);
        if (!icon.isNull()) {
            return icon;
        }
    }
    QIcon icon;
    for (int size : { 16, 24, 32, 48, 64, 128, 256 }) {
        icon.addPixmap(presetIconPixmap(preset.isEmpty() ? QStringLiteral("tara") : preset, size));
    }
    return icon;
}

void applyIconToWindows()
{
    const QIcon icon = currentIcon();
    g_icon = icon;
    QApplication::setWindowIcon(icon);
    for (QWidget* w : QApplication::topLevelWidgets()) {
        if (w->isWindow()) {
            w->setWindowIcon(icon);
        }
    }
}

QString writeCurrentIconFile()
{
    // An .ico is a small header plus PNG images of several sizes; written by hand so it never depends on an image plugin.
    struct Entry {
        int size;
        QByteArray png;
    };
    const QIcon icon = currentIcon();
    QList<Entry> entries;
    for (int size : { 16, 32, 48, 64, 128, 256 }) {
        const QPixmap source = icon.pixmap(QSize(size, size));
        if (source.isNull()) {
            continue;
        }
        QImage square(size, size, QImage::Format_ARGB32);
        square.fill(Qt::transparent);
        {
            // icons without a large variant come back smaller, so they are scaled and centred
            const QPixmap scaled = source.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            QPainter p(&square);
            p.drawPixmap((size - scaled.width()) / 2, (size - scaled.height()) / 2, scaled);
        }
        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        if (square.save(&buffer, "PNG")) {
            entries.append({ size, png });
        }
    }
    if (entries.isEmpty()) {
        return QString();
    }

    QByteArray ico;
    QDataStream out(&ico, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    out << static_cast<quint16>(0) << static_cast<quint16>(1) << static_cast<quint16>(entries.size());  // reserved, type = icon, count
    quint32 offset = 6 + 16 * static_cast<quint32>(entries.size());
    for (const Entry& e : entries) {
        const quint8 dimension = e.size >= 256 ? 0 : static_cast<quint8>(e.size);  // 0 means 256
        out << dimension << dimension << static_cast<quint8>(0) << static_cast<quint8>(0);          // width, height, palette, reserved
        out << static_cast<quint16>(1) << static_cast<quint16>(32);                                  // planes, bits per pixel
        out << static_cast<quint32>(e.png.size()) << offset;                                         // image size, where it starts
        offset += static_cast<quint32>(e.png.size());
    }
    for (const Entry& e : entries) {
        out.writeRawData(e.png.constData(), static_cast<int>(e.png.size()));
    }

    const QString path = QDir(APPLICATION->dataRoot()).filePath(QStringLiteral("tara-icon.ico"));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return QString();
    }
    file.write(ico);
    return file.commit() ? path : QString();
}

bool refreshDesktopShortcut(QString* error)
{
#ifdef Q_OS_WIN
    auto fail = [&](const QString& text) {
        if (error) {
            *error = text;
        }
        return false;
    };
    const QString iconPath = writeCurrentIconFile();
    if (iconPath.isEmpty()) {
        return fail(QObject::tr("Не удалось подготовить файл иконки."));
    }
    const QString desktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (desktop.isEmpty()) {
        return fail(QObject::tr("Не найдена папка рабочего стола."));
    }

    auto* settings = APPLICATION->settings();
    // delete only the shortcut this launcher made earlier
    const QString previous = settings->get("TaraShortcutPath").toString();
    if (!previous.isEmpty() && previous.endsWith(QLatin1String(".lnk"), Qt::CaseInsensitive) && QFile::exists(previous)) {
        QFile::remove(previous);
    }

    const QString linkPath = QDir(desktop).filePath(QStringLiteral("TaraLauncher.lnk"));
    if (QFile::exists(linkPath)) {
        QFile::remove(linkPath);
    }
    const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    const QString workDir = QDir::toNativeSeparators(QCoreApplication::applicationDirPath());

    const HRESULT initResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool ok = false;
    IShellLinkW* link = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, reinterpret_cast<void**>(&link)))) {
        link->SetPath(reinterpret_cast<LPCWSTR>(exe.utf16()));
        link->SetWorkingDirectory(reinterpret_cast<LPCWSTR>(workDir.utf16()));
        link->SetDescription(L"TaraLauncher");
        link->SetIconLocation(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(iconPath).utf16()), 0);
        IPersistFile* file = nullptr;
        if (SUCCEEDED(link->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&file)))) {
            ok = SUCCEEDED(file->Save(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(linkPath).utf16()), TRUE));
            file->Release();
        }
        link->Release();
    }
    if (SUCCEEDED(initResult)) {
        CoUninitialize();
    }
    if (!ok) {
        return fail(QObject::tr("Не удалось создать ярлык на рабочем столе."));
    }
    settings->set("TaraShortcutPath", linkPath);
    // ask Explorer to redraw icons so the new one is visible right away
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
#else
    if (error) {
        *error = QObject::tr("Обновление ярлыка доступно только в Windows.");
    }
    return false;
#endif
}

}  // namespace TaraAppearance
