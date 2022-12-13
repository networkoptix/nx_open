// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "desktop_client_message_processor.h"

#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/data/layout_tour_data.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/layout_tour/showreel_state_manager.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource/layout_snapshot_manager.h>
#include <nx/vms/client/desktop/resource/resource_factory.h>
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
            const auto layout = resource.dynamicCast<QnLayoutResource>();
            return layout && layout->isFile();
        };

    QnResourcePtr ownResource = resourcePool()->getResourceById(resource->getId());

    // Security check. Local layouts must not be overridden by remote. Really that means GUID
    // conflict caused by saving local layouts to server.
    if (isFile(resource) || isFile(ownResource))
        return;

    auto layout = ownResource.dynamicCast<LayoutResource>();
    auto systemContext = dynamic_cast<SystemContext*>(this->systemContext());

    // Ignore remote layout update if user has unsaved data.
    if (layout
        && layout->hasFlags(Qn::remote)
        && NX_ASSERT(!layout->hasFlags(Qn::local))
        && NX_ASSERT(systemContext)
        && systemContext->layoutSnapshotManager()->isChanged(layout))
    {
        return;
    }

    base_type::updateResource(resource, source);
    if (layout && NX_ASSERT(systemContext))
        systemContext->layoutSnapshotManager()->store(layout);
}

void QnDesktopClientMessageProcessor::handleTourAddedOrUpdated(
    const nx::vms::api::LayoutTourData& tour)
{
    auto systemContext = dynamic_cast<SystemContext*>(this->systemContext());

    if (NX_ASSERT(systemContext) && systemContext->showreelStateManager()->isChanged(tour.id))
        return;

    base_type::handleTourAddedOrUpdated(tour);
}
