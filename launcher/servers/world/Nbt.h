// SPDX-License-Identifier: GPL-3.0-only
/*
 *  TaraLauncher - Minecraft Launcher
 *  Tiny NBT reader: just enough to read level.dat and player data files.
 */
#pragma once

#include <QByteArray>
#include <QVariant>

namespace Nbt {

/**
 * Parses uncompressed NBT. Compounds become QVariantMap, lists and arrays become QVariantList,
 * integers qint64, floating point numbers double, strings QString.
 * Returns the content of the root compound, or an invalid QVariant on malformed data.
 */
QVariant parse(const QByteArray& data);

/** Reads a gzip-compressed .dat file (level.dat, playerdata/*.dat). Invalid QVariant on any error. */
QVariant readGzipFile(const QString& path);

}  // namespace Nbt
