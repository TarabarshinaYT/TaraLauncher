// SPDX-License-Identifier: GPL-3.0-only
/*
 *  TaraLauncher - Minecraft Launcher
 *  All settings of one server: launcher-side options, server.properties, players, world map.
 */
#pragma once

#include <QDialog>

class QCheckBox;
class QLineEdit;
class QSpinBox;
class QTabWidget;
class ServerManager;
class ServerProcess;
class ServerPropertiesEditor;
class ConnectionTab;

class ServerSettingsDialog : public QDialog {
    Q_OBJECT
   public:
    ServerSettingsDialog(ServerManager* manager, ServerProcess* server, QWidget* parent = nullptr);

   private slots:
    void save();
    void browseJava();

   private:
    QWidget* buildGeneralTab();

    ServerManager* m_manager = nullptr;
    ServerProcess* m_server = nullptr;
    QTabWidget* m_tabs = nullptr;

    QLineEdit* m_name = nullptr;
    QSpinBox* m_port = nullptr;
    QSpinBox* m_memory = nullptr;
    QLineEdit* m_java = nullptr;
    QLineEdit* m_jvmArgs = nullptr;
    QCheckBox* m_eula = nullptr;
    ServerPropertiesEditor* m_properties = nullptr;
    ConnectionTab* m_connection = nullptr;
};
