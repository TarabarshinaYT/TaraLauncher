// SPDX-License-Identifier: GPL-3.0-only
#include "Nbt.h"

#include <QFile>
#include <cstring>

#include "GZip.h"

namespace {

struct Reader {
    const QByteArray& data;
    int pos = 0;
    bool ok = true;

    bool need(int n)
    {
        if (n < 0 || pos + n > data.size()) {
            ok = false;
        }
        return ok;
    }
    quint8 u8()
    {
        if (!need(1)) {
            return 0;
        }
        return static_cast<quint8>(data[pos++]);
    }
    qint16 i16()
    {
        if (!need(2)) {
            return 0;
        }
        const quint16 v = (static_cast<quint8>(data[pos]) << 8) | static_cast<quint8>(data[pos + 1]);
        pos += 2;
        return static_cast<qint16>(v);
    }
    qint32 i32()
    {
        if (!need(4)) {
            return 0;
        }
        quint32 v = 0;
        for (int i = 0; i < 4; ++i) {
            v = (v << 8) | static_cast<quint8>(data[pos + i]);
        }
        pos += 4;
        return static_cast<qint32>(v);
    }
    qint64 i64()
    {
        if (!need(8)) {
            return 0;
        }
        quint64 v = 0;
        for (int i = 0; i < 8; ++i) {
            v = (v << 8) | static_cast<quint8>(data[pos + i]);
        }
        pos += 8;
        return static_cast<qint64>(v);
    }
    float f32()
    {
        const qint32 bits = i32();
        float f;
        std::memcpy(&f, &bits, 4);
        return f;
    }
    double f64()
    {
        const qint64 bits = i64();
        double d;
        std::memcpy(&d, &bits, 8);
        return d;
    }
    QString str()
    {
        const int len = static_cast<quint16>(i16());
        if (!need(len)) {
            return QString();
        }
        const QString s = QString::fromUtf8(data.constData() + pos, len);
        pos += len;
        return s;
    }
};

QVariant readPayload(Reader& r, int type, int depth);

QVariant readList(Reader& r, int elementType, int count, int depth)
{
    QVariantList list;
    if (count < 0 || count > (1 << 24)) {
        r.ok = false;
        return list;
    }
    for (int i = 0; i < count && r.ok; ++i) {
        list.append(readPayload(r, elementType, depth + 1));
    }
    return list;
}

QVariant readPayload(Reader& r, int type, int depth)
{
    if (depth > 256 || !r.ok) {
        r.ok = false;
        return QVariant();
    }
    switch (type) {
        case 1: return static_cast<qint64>(static_cast<qint8>(r.u8()));
        case 2: return static_cast<qint64>(r.i16());
        case 3: return static_cast<qint64>(r.i32());
        case 4: return static_cast<qint64>(r.i64());
        case 5: return static_cast<double>(r.f32());
        case 6: return r.f64();
        case 7: {  // byte array
            const int n = r.i32();
            if (!r.need(n)) {
                return QVariant();
            }
            r.pos += n;  // contents are not needed
            return QVariantList();
        }
        case 8: return r.str();
        case 9: {
            const int elementType = r.u8();
            const int count = r.i32();
            if (elementType == 0 && count > 0) {
                r.ok = false;
                return QVariant();
            }
            return readList(r, elementType, count, depth);
        }
        case 10: {
            QVariantMap map;
            while (r.ok) {
                const int childType = r.u8();
                if (childType == 0) {
                    break;
                }
                const QString name = r.str();
                map.insert(name, readPayload(r, childType, depth + 1));
            }
            return map;
        }
        case 11: return readList(r, 3, r.i32(), depth);  // int array
        case 12: return readList(r, 4, r.i32(), depth);  // long array
    }
    r.ok = false;
    return QVariant();
}

}  // namespace

namespace Nbt {

QVariant parse(const QByteArray& data)
{
    Reader r{ data };
    if (r.u8() != 10) {
        return QVariant();
    }
    r.str();  // root name, usually empty
    const QVariant root = readPayload(r, 10, 0);
    return r.ok ? root : QVariant();
}

QVariant readGzipFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QVariant();
    }
    QByteArray raw;
    if (!GZip::unzip(file.readAll(), raw)) {
        return QVariant();
    }
    return parse(raw);
}

}  // namespace Nbt
