// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_resource_source.h"

#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

using namespace nx::vms::api;

ServerResourceSource::ServerResourceSource(
    const QnResourcePool* resourcePool, bool displayReducedEdgeServers)
    :
    base_type(),
    m_resourcePool(resourcePool),
    m_displayReducedEdgeServers(displayReducedEdgeServers)
{
    if (!NX_ASSERT(m_resourcePool))
        return;

    connect(m_resourcePool, &QnResourcePool::resourceAdded, this,
        [this](const QnResourcePtr& resource)
        {
            if (!resource->hasFlags(Qn::server) || resource->hasFlags(Qn::fake))
                return;

            const auto server = resource.staticCast<QnMediaServerResource>();
            if (m_displayReducedEdgeServers && server->getServerFlags().testFlag(SF_Edge))
            {
                setupEdgeServerStateTracking(server);
                updateEdgeServerReducedState(server);
                return;
            }

            emit resourceAdded(resource);
        });

    connect(m_resourcePool, &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            if (!resource->hasFlags(Qn::server) || resource->hasFlags(Qn::fake))
                return;

            emit resourceRemoved(resource);

            const auto server = resource.staticCast<QnMediaServerResource>();
            if (server->getServerFlags().testFlag(SF_Edge))
            {
                if (const auto coupledCamera =
                    QnMediaServerResource::getEdgeServerCoupledCamera(server))
                {
                    emit resourceRemoved(coupledCamera.staticCast<QnResource>());
                }
            }
        });
}

QVector<QnResourcePtr> ServerResourceSource::getResources()
{
    if (!NX_ASSERT(m_resourcePool))
        return {};

    QVector<QnResourcePtr> result;
    const auto servers = m_resourcePool->servers();

    std::transform(std::cbegin(servers), std::cend(servers), std::back_inserter(result),
        [this](const QnMediaServerResourcePtr& server)
        {
            if (m_displayReducedEdgeServers && server->getServerFlags().testFlag(SF_Edge)
                && QnMediaServerResource::isHiddenServer(server))
            {
                setupEdgeServerStateTracking(server);
                return QnMediaServerResource::getEdgeServerCoupledCamera(server)
                    .staticCast<QnResource>();
            }
            return server.staticCast<QnResource>();
        });

    return result;
}

void ServerResourceSource::setupEdgeServerStateTracking(
    const QnMediaServerResourcePtr& server) const
{
    connect(server.get(), &QnMediaServerResource::redundancyChanged,
        this, &ServerResourceSource::updateEdgeServerReducedState);

    connect(server.get(), &QnMediaServerResource::edgeServerCanonicalStateChanged,
        this, &ServerResourceSource::updateEdgeServerReducedState);
}

void ServerResourceSource::updateEdgeServerReducedState(const QnResourcePtr& serverResource)
{
    const auto coupledCamera = QnMediaServerResource::getEdgeServerCoupledCamera(serverResource);
    if (QnMediaServerResource::isHiddenServer(serverResource))
    {
        emit resourceRemoved(serverResource);
        emit resourceAdded(coupledCamera.staticCast<QnResource>());
    }
    else
    {
        if (coupledCamera)
            emit resourceRemoved(coupledCamera.staticCast<QnResource>());
        emit resourceAdded(serverResource);
    }
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
