#include "server.h"

QnServer::QnServer(const QString& name)
{
    m_name = name;
    setTypeId(qnResTypePool->getResourceTypeId("", "Server"));
}

QString QnServer::getUniqueId() const
{
    return m_name;
}

bool QnServer::isResourceAccessible()
{
    return true;
}

bool QnServer::updateMACAddress()
{
    // dunno what to do here.

    return true;
}

QnAbstractStreamDataProvider* QnServer::createDataProviderInternal(ConnectionRole role)
{
    return 0;
}
