// SPDX-License-Identifier: GPL-3.0-only
#include "RconClient.h"

#include <QByteArray>
#include <QHostAddress>
#include <QRandomGenerator>
#include <QTcpSocket>

namespace {
constexpr int kTimeoutMs = 2500;
constexpr qint32 kTypeCommand = 2;
constexpr qint32 kTypeAuth = 3;

void putInt(QByteArray& out, qint32 v)
{
    for (int i = 0; i < 4; ++i) {
        out.append(static_cast<char>((v >> (8 * i)) & 0xFF));
    }
}

qint32 getInt(const QByteArray& in, int offset)
{
    qint32 v = 0;
    for (int i = 0; i < 4; ++i) {
        v |= static_cast<qint32>(static_cast<unsigned char>(in[offset + i])) << (8 * i);
    }
    return v;
}

bool writePacket(QTcpSocket& socket, qint32 id, qint32 type, const QString& body)
{
    const QByteArray payload = body.toUtf8();
    QByteArray packet;
    putInt(packet, 10 + payload.size());
    putInt(packet, id);
    putInt(packet, type);
    packet.append(payload);
    packet.append('\0');
    packet.append('\0');
    socket.write(packet);
    return socket.waitForBytesWritten(kTimeoutMs);
}

bool readExactly(QTcpSocket& socket, QByteArray& buffer, int size)
{
    while (buffer.size() < size) {
        if (socket.bytesAvailable() == 0 && !socket.waitForReadyRead(kTimeoutMs)) {
            return false;
        }
        buffer.append(socket.read(size - buffer.size()));
    }
    return true;
}

bool readPacket(QTcpSocket& socket, qint32* id, qint32* type, QString* body)
{
    QByteArray head;
    if (!readExactly(socket, head, 4)) {
        return false;
    }
    const qint32 length = getInt(head, 0);
    if (length < 10 || length > 4096 + 10) {
        return false;
    }
    QByteArray rest;
    if (!readExactly(socket, rest, length)) {
        return false;
    }
    *id = getInt(rest, 0);
    *type = getInt(rest, 4);
    *body = QString::fromUtf8(rest.mid(8, length - 10));
    return true;
}
}  // namespace

namespace Rcon {

QString makePassword()
{
    static const char alphabet[] = "abcdefghijkmnpqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    QString out;
    auto* rng = QRandomGenerator::system();
    for (int i = 0; i < 24; ++i) {
        out.append(QLatin1Char(alphabet[rng->bounded(static_cast<quint32>(sizeof(alphabet) - 1))]));
    }
    return out;
}

bool send(int port, const QString& password, const QString& command, QString* reply, QString* error)
{
    auto fail = [&](const QString& text) {
        if (error) {
            *error = text;
        }
        return false;
    };

    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, static_cast<quint16>(port));
    if (!socket.waitForConnected(kTimeoutMs)) {
        return fail(QObject::tr("Не удалось подключиться к RCON сервера (порт %1). Сервер ещё запускается?").arg(port));
    }

    if (!writePacket(socket, 1, kTypeAuth, password)) {
        return fail(QObject::tr("Ошибка отправки данных RCON."));
    }
    // the answer to a login is a packet with the same id on success and -1 on a wrong password
    for (int attempt = 0; attempt < 2; ++attempt) {
        qint32 id = 0, type = 0;
        QString body;
        if (!readPacket(socket, &id, &type, &body)) {
            return fail(QObject::tr("Сервер не ответил на вход по RCON."));
        }
        if (type == kTypeCommand) {
            if (id == -1) {
                return fail(QObject::tr("RCON: неверный пароль."));
            }
            break;
        }
    }

    if (!writePacket(socket, 2, kTypeCommand, command)) {
        return fail(QObject::tr("Ошибка отправки команды."));
    }
    qint32 id = 0, type = 0;
    QString body;
    // "stop" makes the server close the connection without an answer; that is fine
    if (readPacket(socket, &id, &type, &body) && reply) {
        *reply = body;
    }
    socket.disconnectFromHost();
    return true;
}

}  // namespace Rcon
