// SPDX-License-Identifier: GPL-3.0-only
/*
 *  TaraLauncher - Minecraft Launcher
 *  Window background picture, application icon presets, desktop shortcut refresh.
 */
#pragma once

#include <QIcon>
#include <QObject>
#include <QPixmap>
#include <QStringList>

namespace TaraAppearance {

/** Call once at start: installs the background painter and applies the saved icon. */
void install();

// ---- background ----
/** Re-reads background settings and repaints every window. */
void reloadBackground();

// ---- icon ----
QStringList presetIcons();               // ids, "tara" is the default
QString presetIconName(const QString& id);  // Russian display name
QPixmap presetIconPixmap(const QString& id, int size);
/** The icon selected in settings (a preset or the user's own picture). */
QIcon currentIcon();
/** Sets the icon on the application and on all open windows. */
void applyIconToWindows();
/** Saves `pixmap` as a multi-purpose .ico in the data folder and returns its path (empty on failure). */
QString writeCurrentIconFile();

// ---- desktop shortcut ----
/**
 * Removes the shortcut this launcher created earlier (only that one) and creates a new one
 * on the desktop with the current icon. Windows only.
 */
bool refreshDesktopShortcut(QString* error);

}  // namespace TaraAppearance
