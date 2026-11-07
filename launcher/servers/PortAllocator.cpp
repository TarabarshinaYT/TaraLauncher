// SPDX-License-Identifier: GPL-3.0-only
#include "PortAllocator.h"

#include <QHostAddress>
#include <QTcpServer>

namespace ServerPorts {

bool isFree(int port)
{
    if (port < 1 || port > 65535) {
        return false;
    }
    QTcpServer probe;
    const bool ok = probe.listen(QHostAddress::Any, static_cast<quint16>(port));
    probe.close();
    return ok;
}

int findFree(const QSet<int>& reserved, int start)
{
    for (int port = start; port <= 65535; ++port) {
        if (reserved.contains(port)) {
            continue;
        }
        if (isFree(port)) {
            return port;
        }
    }
    return -1;
}

}  // namespace ServerPorts
