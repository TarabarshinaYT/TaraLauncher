// SPDX-License-Identifier: GPL-3.0-only
/*
 *  TaraLauncher - Minecraft Launcher
 *  Form editor for server.properties. Every setting has a Russian label and a "?" explanation.
 */
#pragma once

#include <QMap>
#include <QWidget>

#include "servers/ServerProperties.h"

class QPlainTextEdit;

class ServerPropertiesEditor : public QWidget {
    Q_OBJECT
   public:
    explicit ServerPropertiesEditor(const QString& serverName, QWidget* parent = nullptr);

    /** Loads server.properties from the given file (a missing file shows the defaults). */
    bool load(const QString& path);
    /** Writes the changes back, keeping comments and settings the form does not know. */
    bool save();
    bool isModified() const { return m_modified; }

   signals:
    void modified();

   private:
    struct Def;
    void build(const QString& serverName);
    void markModified();

    QString m_path;
    ServerProperties m_props;
    QString m_serverName;
    bool m_modified = false;
    bool m_loading = false;

    // key -> widget that edits it (QCheckBox, QSpinBox, QComboBox or QLineEdit)
    QMap<QString, QWidget*> m_fields;
    QPlainTextEdit* m_other = nullptr;
};
