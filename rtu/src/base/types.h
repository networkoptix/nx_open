
#pragma once

#include <QUuid>
#include <QObject>
#include <QVector>

namespace rtu
{
    typedef QHash<int, QByteArray> Roles;

    class ServerInfo;
    typedef QVector<ServerInfo *> ServerInfoList;

    typedef QVector<QUuid> IDsVector;
}
