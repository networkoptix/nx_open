// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_system_cameras_source.h"

#include <QtCore/QPointer>

#include <core/resource/camera_resource.h>
#include <network/base_system_description.h>
#include <nx/utils/scoped_connections.h>
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
    nx::utils::ScopedConnections connections;

    void init()
    {
        connections << appContext()->cloudCrossSystemManager()->requestSystemContext(
            systemId,
            [this](CloudCrossSystemContext* systemContext) { connectToContext(systemContext); });
    }

    void connectToContext(CloudCrossSystemContext* systemContext)
    {
        connections << QObject::connect(systemContext,
            &CloudCrossSystemContext::camerasAdded,
            [=](const QnVirtualCameraResourceList& cameras)
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
            [=](const QnVirtualCameraResourceList& cameras)
            {
                NX_VERBOSE(this,
                    "%1 cameras removed from %2", cameras.size(), *systemContext);

                if (!systemContext->isSystemReadyToUse())
                    return;

                auto handler = q->removeKeyHandler;
                for (auto& camera: cameras)
                    (*handler)(camera);
            });

        const auto handleSystemStateChanges =
            [=]()
            {
                if (systemContext->isSystemReadyToUse())
                {
                    auto cameras = systemContext->cameras();
                    q->setKeysHandler(
                        QVector<QnResourcePtr>(cameras.cbegin(), cameras.cend()));
                }
                else
                {
                    q->setKeysHandler({});
                }
            };

        connections << QObject::connect(
            systemContext->systemDescription(),
            &QnBaseSystemDescription::onlineStateChanged,
            handleSystemStateChanges);

        connections << QObject::connect(
            systemContext,
            &CloudCrossSystemContext::statusChanged,
            handleSystemStateChanges);

        auto cameras = systemContext->cameras();
        if (systemContext->isSystemReadyToUse())
        {
            NX_VERBOSE(this,
                "%1 cameras already exist in the %2", cameras.size(), *systemContext);

            q->setKeysHandler(QVector<QnResourcePtr>(cameras.cbegin(), cameras.cend()));
        }
    }
};

CloudSystemCamerasSource::CloudSystemCamerasSource(const QString& systemId):
    d(new Private(this, systemId))
{
    initializeRequest =
        [this]
        {
            d->init();
        };
}

CloudSystemCamerasSource::~CloudSystemCamerasSource()
{
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
