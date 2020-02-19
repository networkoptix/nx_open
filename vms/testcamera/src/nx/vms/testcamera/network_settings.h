#pragma once

#include<QtCore/QStringList>

struct NetworkSettings
{
    QStringList localInterfacesToListen;
    int discoveryPort = 0;
    int mediaPort = 0;
};
