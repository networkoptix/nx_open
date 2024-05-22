// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "desktop_camera_stub_controller.h"

#include <client/client_message_processor.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/resource/screen_recording/desktop_resource.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource/resource_descriptor.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/videowall/desktop_camera_preloader_resource.h>
#include <utils/common/id.h>

#include <ranges>

namespace nx::vms::client::desktop {

DesktopCameraStubController::DesktopCameraStubController(
    SystemContext* systemContext,
    QObject* parent)
    :
    QObject(parent),
    SystemContextAware(systemContext)
{
    connect(resourcePool(), &QnResourcePool::resourcesAdded,
        this, &DesktopCameraStubController::atResourcesAdded);

    connect(resourcePool(), &QnResourcePool::resourcesRemoved,
        this, &DesktopCameraStubController::atResourcesRemoved);

    connect(systemContext->userWatcher(), &core::UserWatcher::userChanged,
        this, &DesktopCameraStubController::atUserChanged);

    atResourcesAdded(resourcePool()->getResources());
}

DesktopCameraStubController::~DesktopCameraStubController() = default;

QnVirtualCameraResourcePtr DesktopCameraStubController::createLocalCameraStub(
    const QString& physicalId)
{
    if (m_localCameraStub)
        return m_localCameraStub;

    m_localCameraStub.reset(new DesktopCameraPreloaderResource(
        QnVirtualCameraResource::physicalIdToId(physicalId),
        physicalId));

    NX_VERBOSE(this, "Local desktop camera stub has been created");

    resourcePool()->addResource(m_localCameraStub);

    return m_localCameraStub;
}

void DesktopCameraStubController::atResourcesAdded(const QnResourceList& resources)
{
    for (const auto& resource: resources)
    {
        const auto layout = resource.dynamicCast<LayoutResource>();
        if (!layout)
            continue;

        const auto items = layout->getItems();
        if (items.size() != 1) // Desktop camera screen can only contain single item.
            continue;

        const auto& item = *items.begin();

        if (!isDesktopCameraResource(item.resource))
            continue;

        NX_VERBOSE(this, "New desktop camera layout has been found");

        m_desktopCameraUsage.insert(item.resource.id);

        const auto layoutId = layout->getId();
        m_desktopCameraLayoutConnections[layout->getId()] =
            connect(layout.get(), &LayoutResource::itemRemoved, this,
                [this, layoutId](const QnLayoutResourcePtr& resource,
                    const nx::vms::common::LayoutItemData& item)
                {
                    if (!isDesktopCameraResource(item.resource))
                        return;

                    NX_VERBOSE(this, "Desktop camera item removed from desktop camera layout");

                    disconnectLayoutConnection(layoutId);
                    decreaseUsageCount(item.resource.id);
                });

        auto desktopCameraStub = resourcePool()->getResourceByDescriptor(item.resource);
        if (desktopCameraStub)
            continue;

        desktopCameraStub.reset(new DesktopCameraPreloaderResource(
            item.resource.id,
            getDesktopCameraPhysicalId(item.resource)));

        NX_VERBOSE(this, "Desktop camera stub has been created for added layout");

        resourcePool()->addResource(desktopCameraStub);
    }
}

void DesktopCameraStubController::atResourcesRemoved(const QnResourceList& resources)
{
    for (const auto& resource: resources)
    {
        const auto layout = resource.dynamicCast<LayoutResource>();
        if (!layout)
            continue;

        const auto items = layout->getItems();

        if (items.size() != 1) // Desktop camera screen can only contain single item.
            continue;

        const auto& item = *items.begin();

        if (!isDesktopCameraResource(item.resource))
            continue;

        NX_VERBOSE(this, "Desktop camera layout has been removed");

        disconnectLayoutConnection(layout->getId());
        decreaseUsageCount(item.resource.id);
    }
}

void DesktopCameraStubController::atUserChanged()
{
    m_localCameraStub.reset();
}

void DesktopCameraStubController::decreaseUsageCount(const nx::Uuid& resourceId)
{
    m_desktopCameraUsage.extract(resourceId);
    if (!m_desktopCameraUsage.contains(resourceId))
    {
        NX_VERBOSE(this, "Desktop camera usage counter decreased to zero");

        auto desktopCamera = resourcePool()->getResourceById(resourceId);
        if (desktopCamera && desktopCamera->hasFlags(Qn::fake))
        {
            resourcePool()->removeResource(desktopCamera);
            if (desktopCamera == m_localCameraStub)
                m_localCameraStub.reset();
        }
    }
}

void DesktopCameraStubController::disconnectLayoutConnection(const nx::Uuid& layoutId)
{
    auto connectionIter = m_desktopCameraLayoutConnections.find(layoutId);
    if (connectionIter != m_desktopCameraLayoutConnections.end())
    {
        QObject::disconnect(*connectionIter);
        m_desktopCameraLayoutConnections.erase(connectionIter);
    }
}

} // namespace nx::vms::client::desktop
