
#pragma once

#include <QList>
#include <QObject>

namespace rtu
{
    typedef QHash<int, QByteArray> Roles;

    class ServerInfo;
    typedef QList<ServerInfo *> ServerInfoList;
}
