// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_resource_source.h"

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <utils/camera/edge_device.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

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
            if (m_displayReducedEdgeServers && QnMediaServerResource::isEdgeServer(server))
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
            if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
            {
                if (QnMediaServerResource::isEdgeServer(camera->getParentResource()))
                    emit resourceRemoved(camera);
            }

            if (resource->hasFlags(Qn::server) && !resource->hasFlags(Qn::fake))
                emit resourceRemoved(resource);
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
            if (m_displayReducedEdgeServers && QnMediaServerResource::isHiddenEdgeServer(server))
            {
                setupEdgeServerStateTracking(server);

                const auto childCameras =
                    m_resourcePool->getAllCameras(server, /*ignoreDesktopCameras*/ true);

                return NX_ASSERT(childCameras.size() == 1)
                    ? childCameras.front().staticCast<QnResource>()
                    : server.staticCast<QnResource>();
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
    using namespace nx::vms::common::utils;

    if (QnMediaServerResource::isHiddenEdgeServer(serverResource))
    {
        const auto childCameras =
            m_resourcePool->getAllCameras(serverResource, /*ignoreDesktopCameras*/ true);

        if (!NX_ASSERT(childCameras.size() == 1))
        {
            emit resourceAdded(serverResource);
            return;
        }

        emit resourceRemoved(serverResource);
        emit resourceAdded(childCameras.front().staticCast<QnResource>());
    }
    else
    {
        if (QnMediaServerResource::isEdgeServer(serverResource))
        {
            const auto allCameras =
                m_resourcePool->getAllCameras(QnResourcePtr(), /*ignoreDesktopCameras*/ true);

            const auto server = serverResource.staticCast<QnMediaServerResource>();
            const auto coupledEdgeCameras = allCameras.filtered(
                [server](const QnVirtualCameraResourcePtr& camera)
                {
                    return edge_device::isCoupledEdgeCamera(server, camera);
                });

            for (const auto camera: coupledEdgeCameras)
                emit resourceRemoved(camera.staticCast<QnResource>());
        }

        emit resourceAdded(serverResource);
    }
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
