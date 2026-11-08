// SPDX-License-Identifier: GPL-3.0-only
/*
 *  TaraLauncher - Minecraft Launcher
 *  Settings page: visual style, background picture, application icon.
 */
#pragma once

#include <QWidget>

#include "ui/pages/BasePage.h"

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSlider;

class TaraAppearancePage : public QWidget, public BasePage {
    Q_OBJECT
   public:
    explicit TaraAppearancePage(QWidget* parent = nullptr);

    QString displayName() const override { return tr("Внешний вид Tara"); }
    QIcon icon() const override { return QIcon::fromTheme("appearance"); }
    QString id() const override { return "tara-appearance-settings"; }
    QString helpPage() const override { return QString(); }
    bool apply() override { return true; }  // every change is applied immediately

   private:
    QWidget* buildStyleGroup();
    QWidget* buildBackgroundGroup();
    QWidget* buildIconGroup();
    void pickBackground();
    void pickIcon();
    void selectPreset(const QString& id);
    void saveBackgroundSettings();

    QComboBox* m_theme = nullptr;
    QSlider* m_opacity = nullptr;
    QCheckBox* m_bgEnabled = nullptr;
    QLineEdit* m_bgPath = nullptr;
    QComboBox* m_bgMode = nullptr;
    QSlider* m_bgDim = nullptr;
    QSlider* m_bgBlur = nullptr;
    QButtonGroup* m_presets = nullptr;
    QLabel* m_shortcutStatus = nullptr;
};
