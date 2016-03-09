
#pragma once

#include <QtCore/QObject>

class QnConnectionsHolder
{
public:
    typedef QSharedPointer<QnConnectionsHolder> Pointer;
    static Pointer create();

    QnConnectionsHolder();

    ~QnConnectionsHolder();

    void add(const QMetaObject::Connection &connection);

private:
    typedef QList<QMetaObject::Connection> ConnectionsList;

    ConnectionsList m_connections;
};