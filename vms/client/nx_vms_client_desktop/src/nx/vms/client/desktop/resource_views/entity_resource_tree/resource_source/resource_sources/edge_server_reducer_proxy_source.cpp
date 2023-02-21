// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "edge_server_reducer_proxy_source.h"

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/system_context.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

EdgeServerReducerProxySource::EdgeServerReducerProxySource(
    SystemContext* systemContext,
    const QnResourceAccessSubject& subject,
    std::unique_ptr<AbstractResourceSource> serversSource)
    :
    SystemContextAware(systemContext),
    m_serversSource(std::move(serversSource)),
    m_subject(subject)
{
    if (!NX_ASSERT(m_serversSource && this->systemContext()))
        return;

    connect(m_serversSource.get(), &AbstractResourceSource::resourceAdded, this,
        [this](const QnResourcePtr& resource)
        {
            const auto server = resource.objectCast<QnMediaServerResource>();
            if (!NX_ASSERT(server))
                return;

            if (QnMediaServerResource::isEdgeServer(server))
            {
                setupEdgeServerStateTracking(server);
                updateEdgeServerReducedState(server);
                return;
            }

            emit resourceAdded(resource);
        });

    const auto removeServer =
        [this](const QnResourcePtr& serverResource)
        {
            const auto server = serverResource.objectCast<QnMediaServerResource>();
            if (!NX_ASSERT(server))
                return;

            if (const auto camera = m_reducedServerCameras.take(server))
                emit resourceRemoved(camera);
            else
                emit resourceRemoved(server);
        };

    connect(m_serversSource.get(), &AbstractResourceSource::resourceRemoved,
        this, removeServer);

    const auto notifier = systemContext->resourceAccessManager()->createNotifier();
    notifier->setSubjectId(m_subject.id());

    const auto updateAllEdgeServers =
        [this]()
        {
            const auto servers = resourcePool()->servers();
            const auto edgeServers = servers.filtered(QnMediaServerResource::isEdgeServer);

            for (const auto& edgeServer: edgeServers)
                updateEdgeServerReducedState(edgeServer);
        };

    connect(resourceAccessManager(), &QnResourceAccessManager::resourceAccessReset,
        this, updateAllEdgeServers);

    connect(notifier, &QnResourceAccessManager::Notifier::resourceAccessChanged,
        this, updateAllEdgeServers);

    const auto servers = resourcePool()->servers();
    const auto edgeServers = servers.filtered(QnMediaServerResource::isEdgeServer);

    for (const auto& edgeServer: edgeServers)
    {
        setupEdgeServerStateTracking(edgeServer);

        if (auto camera = cameraToReduceTo(edgeServer))
            m_reducedServerCameras.insert(edgeServer, camera);
    }
}

QVector<QnResourcePtr> EdgeServerReducerProxySource::getResources()
{
    if (!NX_ASSERT(m_serversSource))
        return {};

    QVector<QnResourcePtr> result;
    const auto servers = m_serversSource->getResources();

    std::transform(std::cbegin(servers), std::cend(servers), std::back_inserter(result),
        [this](const QnResourcePtr& server) -> QnResourcePtr
        {
            if (const auto camera = m_reducedServerCameras.value(
                server.objectCast<QnMediaServerResource>()))
            {
                return camera;
            }

            return server;
        });

    return result;
}

void EdgeServerReducerProxySource::setupEdgeServerStateTracking(
    const QnMediaServerResourcePtr& server) const
{
    connect(server.get(), &QnMediaServerResource::redundancyChanged,
        this, &EdgeServerReducerProxySource::updateEdgeServerReducedState);

    connect(server.get(), &QnMediaServerResource::edgeServerCanonicalStateChanged,
        this, &EdgeServerReducerProxySource::updateEdgeServerReducedState);
}

QnVirtualCameraResourcePtr EdgeServerReducerProxySource::cameraToReduceTo(
    const QnMediaServerResourcePtr& server) const
{
    if (!QnMediaServerResource::isHiddenEdgeServer(server))
        return {};

    const auto childCameras =
        resourcePool()->getAllCameras(server, /*ignoreDesktopCameras*/ true);

    return NX_ASSERT(childCameras.size() == 1) && hasAccess(childCameras.front())
        ? childCameras.front()
        : QnVirtualCameraResourcePtr{};
}

void EdgeServerReducerProxySource::updateEdgeServerReducedState(const QnResourcePtr& serverResource)
{
    const auto server = serverResource.objectCast<QnMediaServerResource>();
    if (!NX_ASSERT(server))
        return;

    const auto removeCamera =
        [=]()
        {
            if (const auto camera = m_reducedServerCameras.take(server))
                emit resourceRemoved(camera);
        };

    if (auto camera = cameraToReduceTo(server))
    {
        if (m_reducedServerCameras.value(server) != camera)
        {
            removeCamera();
            m_reducedServerCameras.insert(server, camera);
            emit resourceRemoved(server);
            emit resourceAdded(camera);
        }
    }
    else
    {
        removeCamera();
        emit resourceAdded(server);
    }
}

bool EdgeServerReducerProxySource::hasAccess(const QnResourcePtr& resource) const
{
    return resourceAccessManager()->hasPermission(m_subject, resource, Qn::ReadPermission);
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
