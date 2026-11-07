// SPDX-License-Identifier: GPL-3.0-only
/*
 *  TaraLauncher - Minecraft Launcher
 *  Server component: reading and writing server.properties
 */
#pragma once

#include <QList>
#include <QString>
#include <QStringList>

/**
 * Parser for Minecraft's server.properties that keeps comments, blank lines and
 * the original key order, so saving never destroys what the user wrote by hand.
 * Values are kept exactly as written (no unescaping).
 */
class ServerProperties {
   public:
    ServerProperties() = default;

    /** Loads the file. A missing file is not an error: it just yields an empty set. */
    bool load(const QString& path);
    /** Writes the file atomically. Returns false on failure. */
    bool save(const QString& path) const;

    bool contains(const QString& key) const;
    QString value(const QString& key, const QString& fallback = QString()) const;
    bool boolValue(const QString& key, bool fallback = false) const;
    int intValue(const QString& key, int fallback = 0) const;

    /** Replaces the value if the key exists, otherwise appends a new line. */
    void set(const QString& key, const QString& value);
    void setBool(const QString& key, bool value);
    void setInt(const QString& key, int value);
    void remove(const QString& key);

    QStringList keys() const;

   private:
    struct Line {
        QString raw;    // used verbatim for comments and blank lines
        QString key;    // empty for comments and blank lines
        QString value;
    };
    QList<Line> m_lines;
};
