
#include "disconnect_helper.h"

QnDisconnectHelperPtr QnDisconnectHelper::create()
{
    return QnDisconnectHelperPtr(new QnDisconnectHelper());
}

QnDisconnectHelper::QnDisconnectHelper()
    : m_connections()
{}

QnDisconnectHelper::~QnDisconnectHelper()
{
    for (const auto connection: m_connections)
        QObject::disconnect(connection);
}

void QnDisconnectHelper::add(const QMetaObject::Connection &connection)
{
    m_connections.append(connection);
}

QnDisconnectHelper& operator << (QnDisconnectHelper &target
    , const QMetaObject::Connection &connection)
{
    target.add(connection);
    return target;
}
