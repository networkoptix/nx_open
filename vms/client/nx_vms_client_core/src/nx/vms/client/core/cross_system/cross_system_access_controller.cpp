// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cross_system_access_controller.h"

#include <nx/vms/client/core/application_context.h>

#include "cloud_cross_system_context.h"
#include "cloud_cross_system_manager.h"
#include "cross_system_camera_resource.h"
#include "cross_system_layout_resource.h"

namespace nx::vms::client::core {

CrossSystemAccessController::CrossSystemAccessController(
    SystemContext* systemContext, QObject* parent):
    base_type(systemContext, parent)
{
}

CrossSystemAccessController::~CrossSystemAccessController()
{
}

Qn::Permissions CrossSystemAccessController::calculatePermissions(
    const QnResourcePtr& targetResource) const
{
    if (const auto cloudLayout = targetResource.objectCast<CrossSystemLayoutResource>())
    {
        // User can fully control his Cloud Layouts.
        return cloudLayout->locked()
            ? (Qn::FullLayoutPermissions & ~(Qn::AddRemoveItemsPermission | Qn::WriteNamePermission))
            : Qn::FullLayoutPermissions;
    }

    // If a system the resource belongs to isn't connected yet, user must be able to view thumbnails
    // with the appropriate informations.
    if (const auto crossSystemCamera = targetResource.objectCast<CrossSystemCameraResource>())
    {
        const auto systemId = crossSystemCamera->systemId();
        const auto context = appContext()->cloudCrossSystemManager()->systemContext(systemId);

        if (context && !context->isSystemReadyToUse())
            return Qn::ViewContentPermission;
    }

    return base_type::calculatePermissions(targetResource);
}

} // namespace nx::vms::client::core
