
#pragma once

#include <QUuid>
#include <QObject>
#include <QVector>
#include <QString>
#include <QScopedPointer>

namespace rtu
{
    typedef QHash<int, QByteArray> Roles;

    class ServerInfo;
    typedef QVector<ServerInfo *> ServerInfoList;

    typedef QVector<QUuid> IDsVector;

    ///

    typedef QScopedPointer<QString> StringPointer;
    typedef QScopedPointer<int> IntPointer;
}
