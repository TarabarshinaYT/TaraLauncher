// SPDX-License-Identifier: GPL-3.0-only
/*
 *  TaraLauncher - Minecraft Launcher
 *  "Mods / Plugins" tab: search on Modrinth, install into the server folder, list and remove installed files.
 */
#pragma once

#include <QNetworkAccessManager>
#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class ServerProcess;

class ContentTab : public QWidget {
    Q_OBJECT
   public:
    explicit ContentTab(ServerProcess* server, QWidget* parent = nullptr);

   private:
    QStringList loaders() const;
    QString contentDir() const;
    void search();
    void installSelected();
    void removeSelected();
    void reloadInstalled();
    void setStatus(const QString& text);

    ServerProcess* m_server = nullptr;
    QNetworkAccessManager m_net;
    QLineEdit* m_query = nullptr;
    QListWidget* m_results = nullptr;
    QListWidget* m_installed = nullptr;
    QPushButton* m_install = nullptr;
    QLabel* m_status = nullptr;
};
