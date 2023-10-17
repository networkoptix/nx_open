// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_resource_source.h"

#include <core/resource/media_server_resource.h>
#include <nx/vms/client/core/resource/session_resources_signal_listener.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/camera_resource_index.h>
#include <nx/vms/client/desktop/system_context.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

CameraResourceSource::CameraResourceSource(
    const CameraResourceIndex* camerasIndex,
    const QnMediaServerResourcePtr& server)
    :
    base_type(),
    m_camerasIndex(camerasIndex),
    m_server(server)
{
    if (!NX_ASSERT(m_camerasIndex))
        return;

    if (!m_server)
    {
        connect(m_camerasIndex, &CameraResourceIndex::cameraAdded, this,
            [this](const QnResourcePtr& camera) { emit resourceAdded(camera); });

        connect(m_camerasIndex, &CameraResourceIndex::cameraRemoved, this,
            [this](const QnResourcePtr& camera) { emit resourceRemoved(camera); });
    }
    else
    {
        connect(m_camerasIndex, &CameraResourceIndex::cameraAddedToServer, this,
            [this](const QnResourcePtr& camera, const QnResourcePtr& server)
            {
                if (server == m_server)
                    emit resourceAdded(camera);
            });

        connect(m_camerasIndex, &CameraResourceIndex::cameraRemovedFromServer, this,
            [this](const QnResourcePtr& camera, const QnResourcePtr& server)
            {
                if (server == m_server)
                    emit resourceRemoved(camera);
            });

        auto systemContext = SystemContext::fromResource(m_server);
        NX_CRITICAL(systemContext);
        // TODO: #sivanov There should be more elegant way to handle unit tests limitations.
        // Message processor does not exist in unit tests.
        if (systemContext->messageProcessor())
        {
            auto serverWatcher = new core::SessionResourcesSignalListener<QnMediaServerResource>(
                systemContext,
                this);
            serverWatcher->setOnRemovedHandler(
                [this](const QnMediaServerResourceList& servers)
                {
                    if (m_server && servers.contains(m_server))
                        m_server.reset();
                });
            serverWatcher->start();
        }
    }
}

QVector<QnResourcePtr> CameraResourceSource::getResources()
{
    if (m_server)
        return m_camerasIndex->camerasOnServer(m_server);

    return m_camerasIndex->allCameras();
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
