#pragma once

#include <QtCore/QStringList>

struct NetworkOptions
{
    /** If empty, all local interfaces are being listened to. */
    QStringList localInterfacesToListen;

    int discoveryPort = 0;
    int mediaPort = 0;
    bool reuseDiscoveryPort = false;
};
