// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QSet>

namespace ServerPorts {
/** True if nothing on this machine is currently listening on the port. */
bool isFree(int port);
/**
 * Finds the first port >= start that is free and not in `reserved`
 * (ports already assigned to other configured servers, running or not).
 * Returns -1 if nothing is found up to 65535.
 */
int findFree(const QSet<int>& reserved, int start = 25565);
}  // namespace ServerPorts
