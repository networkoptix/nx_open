// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "desktop_client_message_processor.h"

#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/data/showreel_data.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource/layout_snapshot_manager.h>
#include <nx/vms/client/desktop/resource/resource_factory.h>
#include <nx/vms/client/desktop/showreel/showreel_state_manager.h>
#include <nx/vms/client/desktop/system_context.h>

using namespace nx::vms::client::desktop;

QnResourceFactory* QnDesktopClientMessageProcessor::getResourceFactory() const
{
    return appContext()->resourceFactory();
}

void QnDesktopClientMessageProcessor::updateResource(
    const QnResourcePtr& resource,
    ec2::NotificationSource source)
{
    auto isFile =
        [](const QnResourcePtr &resource)
        {
            const auto layout = resource.objectCast<QnLayoutResource>();
            return layout && layout->isFile();
        };

    QnResourcePtr ownResource = resourcePool()->getResourceById(resource->getId());

    // Security check. Local layouts must not be overridden by remote. Really that means GUID
    // conflict caused by saving local layouts to server.
    if (isFile(resource) || isFile(ownResource))
        return;

    const auto layout = ownResource.objectCast<LayoutResource>();
    const auto remoteLayout = resource.objectCast<QnLayoutResource>();
    const auto systemContext = dynamic_cast<SystemContext*>(this->systemContext());

    if (!remoteLayout //< Not a layout is being processed.
        || !layout //< A newly created layout is being processed.
        || !NX_ASSERT(systemContext))
    {
        base_type::updateResource(resource, source);
        return;
    }

    // Update snapshot early to ensure all indirect access rights are resolved by the time
    // the actual resource is updated.
    systemContext->layoutSnapshotManager()->update(layout, remoteLayout);
    base_type::updateResource(resource, source);
    systemContext->layoutSnapshotManager()->store(layout);
}

void QnDesktopClientMessageProcessor::handleTourAddedOrUpdated(
    const nx::vms::api::ShowreelData& tour)
{
    auto systemContext = dynamic_cast<SystemContext*>(this->systemContext());

    if (NX_ASSERT(systemContext) && systemContext->showreelStateManager()->isChanged(tour.id))
        return;

    base_type::handleTourAddedOrUpdated(tour);
}
