// SPDX-License-Identifier: GPL-3.0-only
/*
 *  TaraLauncher - Minecraft Launcher
 *  Dialog for creating a local Minecraft server, styled after "New Instance".
 */
#pragma once

#include <QDialog>

#include "BaseVersion.h"
#include "servers/ServerConfig.h"

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLineEdit;
class QSpinBox;
class VersionSelectWidget;

class NewServerDialog : public QDialog {
    Q_OBJECT
   public:
    explicit NewServerDialog(QWidget* parent = nullptr);

    /** Valid after the dialog was accepted. Port 25565 is only a wish: ServerManager::create() picks a free one. */
    ServerConfig config() const;

   private slots:
    void filterChanged();
    void versionChanged(BaseVersion::Ptr version);
    void typeChanged();
    void updateOkButton();

   private:
    VersionSelectWidget* m_versions = nullptr;
    QCheckBox* m_releases = nullptr;
    QCheckBox* m_snapshots = nullptr;
    QComboBox* m_type = nullptr;
    QLineEdit* m_name = nullptr;
    QSpinBox* m_memory = nullptr;
    QCheckBox* m_eula = nullptr;
    QDialogButtonBox* m_buttons = nullptr;
    BaseVersion::Ptr m_selected;
    bool m_nameEditedByUser = false;
};
