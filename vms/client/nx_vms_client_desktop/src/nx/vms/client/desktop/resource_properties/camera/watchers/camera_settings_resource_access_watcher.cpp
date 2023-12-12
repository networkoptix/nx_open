// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_settings_resource_access_watcher.h"

#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>

#include "../flux/camera_settings_dialog_store.h"

namespace nx::vms::client::desktop {

class CameraSettingsResourceAccessWatcher::Private
{
public:
    CameraSettingsDialogStore* const store;
    const QPointer<nx::vms::client::core::SystemContext> systemContext;
    QSet<QnVirtualCameraResourcePtr> cameras;

public:
    void updatePermissions()
    {
        if (!systemContext)
            return;

        const auto getPermissions =
            [this](const QnVirtualCameraResourcePtr& camera) -> Qn::Permissions
            {
                return camera && camera->systemContext() == systemContext
                    ? systemContext->accessController()->permissions(camera)
                    : Qn::NoPermissions;
            };

        store->setPermissions(cameras.size() == 1
            ? getPermissions(*cameras.cbegin())
            : Qn::NoPermissions);

        store->setAllCamerasEditable(std::all_of(cameras.cbegin(), cameras.cend(),
            [=](const QnVirtualCameraResourcePtr& camera)
            {
                return getPermissions(camera).testFlags(
                    Qn::ReadWriteSavePermission | Qn::WriteNamePermission);
            }));
    };
};

CameraSettingsResourceAccessWatcher::CameraSettingsResourceAccessWatcher(
    CameraSettingsDialogStore* store,
    SystemContext* systemContext,
    ui::action::Manager* menu,
    QObject* parent)
    :
    base_type(parent),
    d(new Private{.store = store, .systemContext = systemContext})
{
    if (!NX_ASSERT(store && systemContext))
        return;

    connect(systemContext->accessController(), &core::AccessController::permissionsMaybeChanged,
        this, [this]() { d->updatePermissions(); });

    if (const auto& action = menu->action(ui::action::ToggleScreenRecordingAction))
    {
        connect(action, &QAction::toggled, this,
            [this](bool checked){ d->store->setScreenRecordingOn(checked); });
    }

    const auto updateUserAccessRightsInfo =
        [this]()
        {
            if (!d->systemContext)
                return;

            const auto user = d->systemContext->accessController()->user();
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
                d->systemContext->resourceAccessManager()->hasAccessToAllCameras(
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

void CameraSettingsResourceAccessWatcher::setCameras(
    const QnVirtualCameraResourceList& cameras)
{
    const auto cameraSet = nx::utils::toQSet(cameras);
    if (d->cameras == cameraSet)
    {
        // We need to update the store even if cameras were not changed.
        d->updatePermissions();
        return;
    }

    d->cameras = cameraSet;
    d->updatePermissions();
}

} // namespace nx::vms::client::desktop
