// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_layout_change_validator.h"

#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <nx/vms/client/desktop/access/caching_access_controller.h>
#include <nx/vms/client/desktop/ui/messages/resources_messages.h>
#include <ui/workbench/workbench_context.h>

QnWorkbenchLayoutsChangeValidator::QnWorkbenchLayoutsChangeValidator(QnWorkbenchContext* context):
    QnWorkbenchContextAware(context)
{
}

bool QnWorkbenchLayoutsChangeValidator::confirmChangeVideoWallLayout(
    const QnLayoutResourcePtr& layout,
    const QnResourceList& removedResources) const
{
    if (!NX_ASSERT(accessController()->user()) || accessController()->hasPowerUserPermissions())
        return true;

    const auto videowall = layout->getParentResource().objectCast<QnVideoWallResource>();

    QnResourceList inaccessible = removedResources.filtered(
        [this, videowall](const QnResourcePtr& resource) -> bool
        {
            const auto details = resourceAccessManager()->accessDetails(
                accessController()->user()->getId(), resource, nx::vms::api::AccessRight::view);

            for (const auto& providers: details)
            {
                if (providers.size() != 1 || *providers.cbegin() != videowall)
                    return false;
            }

            return true;
        });

    if (inaccessible.isEmpty())
        return true;

    return nx::vms::client::desktop::ui::messages::Resources::changeVideoWallLayout(
        mainWindowWidget(), inaccessible);
}
