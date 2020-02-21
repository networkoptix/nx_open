#pragma once

#include <QtCore/QStringList>

struct NetworkOptions
{
    QStringList localInterfacesToListen;
    int discoveryPort = 0;
    int mediaPort = 0;
    bool reuseDiscoveryPort = false;
};
