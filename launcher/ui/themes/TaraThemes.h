// SPDX-License-Identifier: GPL-3.0-only
/*
 *  TaraLauncher - Minecraft Launcher
 *  Extra visual styles: Glass, Smooth, Neon, Minimal.
 */
#pragma once

#include "FusionTheme.h"

class TaraTheme : public FusionTheme {
   public:
    enum class Kind { Glass, Smooth, Neon, Minimal };

    explicit TaraTheme(Kind kind) : m_kind(kind) {}

    QString id() override;
    QString name() override;
    QString tooltip() override;
    bool hasStyleSheet() override { return true; }
    QString appStyleSheet() override;
    QPalette colorScheme() override;
    QColor fadeColor() override;
    double fadeAmount() override { return 0.5; }

   private:
    Kind m_kind;
};
