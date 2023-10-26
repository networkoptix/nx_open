// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_settings_resource_access_watcher.h"

#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/action.h>

#include "../flux/camera_settings_dialog_store.h"

namespace nx::vms::client::desktop {

class CameraSettingsResourceAccessWatcher::Private
{
public:
    CameraSettingsDialogStore* store = nullptr;
    QnVirtualCameraResourcePtr camera;
    core::AccessController::NotifierPtr cameraAccessNotifier;

public:
    void updateCameraPermissions()
    {
        if (const auto systemContext = SystemContext::fromResource(camera))
            store->setPermissions(systemContext->accessController()->permissions(camera));
    };
};

CameraSettingsResourceAccessWatcher::CameraSettingsResourceAccessWatcher(
    CameraSettingsDialogStore* store,
    SystemContext* systemContext,
    menu::Manager* menu,
    QObject* parent)
    :
    base_type(parent),
    d(new Private{.store = store})
{
    if (!NX_ASSERT(store))
        return;

    if (const auto& action = menu->action(menu::ToggleScreenRecordingAction))
    {
        connect(action, &QAction::toggled, this,
            [this](bool checked){ d->store->setScreenRecordingOn(checked); });
    }
    const auto updateUserAccessRightsInfo =
        [this, systemContext]()
        {
            const auto user = systemContext->accessController()->user();
            if (!user)
            {
                d->store->setHasEditAccessRightsForAllCameras(false);
                return;
            }

            if (user->isAdministrator())
            {
                d->store->setHasEditAccessRightsForAllCameras(true);
                return;
            }

            d->store->setHasEditAccessRightsForAllCameras(
                systemContext->resourceAccessManager()->hasAccessToAllCameras(
                    user->getId(),
                    nx::vms::api::AccessRight::edit));
        };

    connect(systemContext->accessController(), &core::AccessController::permissionsMaybeChanged,
        this, updateUserAccessRightsInfo);

    updateUserAccessRightsInfo();
}

CameraSettingsResourceAccessWatcher::~CameraSettingsResourceAccessWatcher()
{
}

void CameraSettingsResourceAccessWatcher::setCamera(
    const QnVirtualCameraResourcePtr& camera)
{
    if (d->camera == camera)
    {
        // We need to update the store even if the camera was not changed.
        d->updateCameraPermissions();
        return;
    }

    d->cameraAccessNotifier = {};
    d->camera = camera;

    if (!camera)
        return;

    d->cameraAccessNotifier =
        SystemContext::fromResource(camera)->accessController()->createNotifier(camera);

    connect(d->cameraAccessNotifier.get(), &core::AccessController::Notifier::permissionsChanged,
        this, [this]() { d->updateCameraPermissions(); });

    d->updateCameraPermissions();
}

} // namespace nx::vms::client::desktop
