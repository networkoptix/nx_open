// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_settings_resource_access_watcher.h"

#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/system_context.h>
#include <ui/workbench/workbench_access_controller.h>

#include "../flux/camera_settings_dialog_store.h"

namespace nx::vms::client::desktop {

CameraSettingsResourceAccessWatcher::CameraSettingsResourceAccessWatcher(
    CameraSettingsDialogStore* store,
    SystemContext* systemContext,
    QObject* parent)
    :
    base_type(parent)
{
    if (!NX_ASSERT(store))
        return;

    const auto updateUserAccessRightsInfo =
        [systemContext, store]()
        {
            const auto user = systemContext->accessController()->user();
            if (!user)
            {
                store->setHasEditAccessRightsForAllCameras(false);
                return;
            }

            if (user->isAdministrator())
            {
                store->setHasEditAccessRightsForAllCameras(true);
                return;
            }

            store->setHasEditAccessRightsForAllCameras(
                systemContext->resourceAccessManager()->hasAccessToAllCameras(
                    user->getId(),
                    nx::vms::api::AccessRight::edit));
        };

    const auto notifier = systemContext->resourceAccessManager()->createNotifier(this);
    if (const auto user = systemContext->accessController()->user())
        notifier->setSubjectId(user->getId());

    connect(systemContext->accessController(), &QnWorkbenchAccessController::userChanged, this,
        [notifier, updateUserAccessRightsInfo](const QnUserResourcePtr& user)
        {
            updateUserAccessRightsInfo();
            notifier->setSubjectId(user ? user->getId() : QnUuid());
        });

    connect(notifier, &QnResourceAccessManager::Notifier::resourceAccessChanged,
        this, updateUserAccessRightsInfo);

    updateUserAccessRightsInfo();
}

} // namespace nx::vms::client::desktop
