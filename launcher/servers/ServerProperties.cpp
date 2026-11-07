// SPDX-License-Identifier: GPL-3.0-only
#include "ServerProperties.h"

#include <QFile>
#include <QSaveFile>
#include <QTextStream>

bool ServerProperties::load(const QString& path)
{
    m_lines.clear();
    QFile file(path);
    if (!file.exists()) {
        return true;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    while (!in.atEnd()) {
        const QString text = in.readLine();
        Line line;
        const QString trimmed = text.trimmed();
        const int eq = text.indexOf('=');
        if (trimmed.isEmpty() || trimmed.startsWith('#') || trimmed.startsWith('!') || eq < 0) {
            line.raw = text;
        } else {
            line.key = text.left(eq).trimmed();
            line.value = text.mid(eq + 1);
        }
        m_lines.append(line);
    }
    return true;
}

bool ServerProperties::save(const QString& path) const
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    for (const Line& line : m_lines) {
        if (line.key.isEmpty()) {
            out << line.raw << '\n';
        } else {
            out << line.key << '=' << line.value << '\n';
        }
    }
    out.flush();
    return file.commit();
}

bool ServerProperties::contains(const QString& key) const
{
    for (const Line& line : m_lines) {
        if (line.key == key) {
            return true;
        }
    }
    return false;
}

QString ServerProperties::value(const QString& key, const QString& fallback) const
{
    for (const Line& line : m_lines) {
        if (line.key == key) {
            return line.value;
        }
    }
    return fallback;
}

bool ServerProperties::boolValue(const QString& key, bool fallback) const
{
    const QString v = value(key).trimmed().toLower();
    if (v == "true") {
        return true;
    }
    if (v == "false") {
        return false;
    }
    return fallback;
}

int ServerProperties::intValue(const QString& key, int fallback) const
{
    bool ok = false;
    const int v = value(key).trimmed().toInt(&ok);
    return ok ? v : fallback;
}

void ServerProperties::set(const QString& key, const QString& value)
{
    for (Line& line : m_lines) {
        if (line.key == key) {
            line.value = value;
            return;
        }
    }
    Line line;
    line.key = key;
    line.value = value;
    m_lines.append(line);
}

void ServerProperties::setBool(const QString& key, bool value)
{
    set(key, value ? QStringLiteral("true") : QStringLiteral("false"));
}

void ServerProperties::setInt(const QString& key, int value)
{
    set(key, QString::number(value));
}

void ServerProperties::remove(const QString& key)
{
    for (int i = 0; i < m_lines.size(); ++i) {
        if (m_lines[i].key == key) {
            m_lines.removeAt(i);
            return;
        }
    }
}

QStringList ServerProperties::keys() const
{
    QStringList out;
    for (const Line& line : m_lines) {
        if (!line.key.isEmpty()) {
            out << line.key;
        }
    }
    return out;
}
