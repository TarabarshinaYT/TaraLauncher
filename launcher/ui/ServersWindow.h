// SPDX-License-Identifier: GPL-3.0-only
/*
 *  TaraLauncher - Minecraft Launcher
 *  Window that lists local servers and lets several of them run at once.
 */
#pragma once

#include <QWidget>

#include "servers/ServerProcess.h"

class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPlainTextEdit;
class QPushButton;
class ServerManager;

class ServersWindow : public QWidget {
    Q_OBJECT
   public:
    explicit ServersWindow(ServerManager* manager, QWidget* parent = nullptr);

   private slots:
    void createServer();
    void startSelected();
    void stopSelected();
    void restartSelected();
    void deleteSelected();
    void openFolder();
    void sendCommand();
    void selectionChanged();
    void refreshItem(ServerProcess* server);

   private:
    ServerProcess* selected() const;
    QListWidgetItem* itemFor(const QString& id) const;
    void addItem(ServerProcess* server);
    void updateButtons();
    static QString statusText(ServerProcess::Status status);

    ServerManager* m_manager = nullptr;
    QListWidget* m_list = nullptr;
    QPlainTextEdit* m_console = nullptr;
    QLineEdit* m_command = nullptr;
    QLabel* m_warning = nullptr;
    QPushButton* m_start = nullptr;
    QPushButton* m_stop = nullptr;
    QPushButton* m_restart = nullptr;
    QPushButton* m_folder = nullptr;
    QPushButton* m_delete = nullptr;
};
