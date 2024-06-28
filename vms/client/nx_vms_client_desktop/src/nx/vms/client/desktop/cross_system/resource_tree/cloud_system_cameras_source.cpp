// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_system_cameras_source.h"

#include <QtCore/QPointer>

#include <core/resource/camera_resource.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/core/system_finder/system_description.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/cross_system/cloud_cross_system_context.h>
#include <nx/vms/client/desktop/cross_system/cloud_cross_system_manager.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

struct CloudSystemCamerasSource::Private
{
    Private(CloudSystemCamerasSource* owner, const QString& systemId):
        q(owner),
        systemId(systemId)
    {
    }

    CloudSystemCamerasSource* const q;
    const QString systemId;
    QPointer<CloudCrossSystemContext> systemContext;
    nx::utils::ScopedConnections connections;

    void connectToContext()
    {
        systemContext = appContext()->cloudCrossSystemManager()->systemContext(systemId);
        if (!NX_ASSERT(systemContext))
            return;

        NX_VERBOSE(this, "Connect to %1", *systemContext);

        connections << QObject::connect(systemContext,
            &CloudCrossSystemContext::camerasAdded,
            [this](const QnVirtualCameraResourceList& cameras)
            {
                NX_VERBOSE(this, "%1 cameras added to %2", cameras.size(), *systemContext);

                if (!systemContext->isSystemReadyToUse())
                    return;

                auto handler = q->addKeyHandler;
                for (auto& camera: cameras)
                    (*handler)(camera);
            });

        connections << QObject::connect(systemContext,
            &CloudCrossSystemContext::camerasRemoved,
            [this](const QnVirtualCameraResourceList& cameras)
            {
                NX_VERBOSE(this, "%1 cameras removed from %2", cameras.size(), *systemContext);

                if (!systemContext->isSystemReadyToUse())
                    return;

                auto handler = q->removeKeyHandler;
                for (auto& camera: cameras)
                    (*handler)(camera);
            });

        const auto handleSystemStateChanges =
            [this]
            {
                if (systemContext->isSystemReadyToUse())
                {
                    auto cameras = systemContext->cameras();
                    q->setKeysHandler(QVector<QnResourcePtr>(cameras.cbegin(), cameras.cend()));
                }
                else
                {
                    q->setKeysHandler({});
                }
            };

        connections << QObject::connect(
            systemContext->systemDescription().get(),
            &core::SystemDescription::onlineStateChanged,
            handleSystemStateChanges);

        connections << QObject::connect(
            systemContext,
            &CloudCrossSystemContext::statusChanged,
            handleSystemStateChanges);

        auto cameras = systemContext->cameras();
        if (systemContext->isSystemReadyToUse())
        {
            NX_VERBOSE(this, "%1 cameras already exist in the %2", cameras.size(), *systemContext);
            q->setKeysHandler(QVector<QnResourcePtr>(cameras.cbegin(), cameras.cend()));
        }
    };
};

CloudSystemCamerasSource::CloudSystemCamerasSource(const QString& systemId):
    d(new Private(this, systemId))
{
    initializeRequest =
        [this]
        {
            // Context must already exist.
            d->connectToContext();
        };
}

CloudSystemCamerasSource::~CloudSystemCamerasSource()
{
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
