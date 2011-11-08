#include "processor.h"

#include "core/resourcemanagment/resource_pool.h"

QnAppserverResourceProcessor::QnAppserverResourceProcessor(const QnId& serverId, const QHostAddress& host, const QAuthenticator& auth, QnResourceFactory& resourceFactory)
    : m_appServer(host, auth, resourceFactory),
      m_serverId(serverId)
{
}

void QnAppserverResourceProcessor::processResources(const QnResourceList &resources)
{
    QnResourceList cameras;

    foreach (QnResourcePtr resource, resources)
    {
        QnNetworkResourcePtr networkResource = resource.dynamicCast<QnNetworkResource>();

        if (networkResource.isNull())
            continue;

        m_appServer.addCamera(*networkResource, m_serverId, cameras);
    }

    QnResourcePool::instance()->addResources(cameras);
}
