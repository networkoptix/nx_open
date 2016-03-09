
#include "connections_holder.h"

QnConnectionsHolder::QnConnectionsHolder()
    : m_connections()
{}

QnConnectionsHolder::~QnConnectionsHolder()
{
    for (const auto connection: m_connections)
        QObject::disconnect(connection);
}

void QnConnectionsHolder::add(const QMetaObject::Connection &connection)
{
    m_connections.append(connection);
}
