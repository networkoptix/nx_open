// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_settings_resource_access_watcher.h"

#include <ranges>

#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/desktop/menu/action.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/system_context.h>

#include "../flux/camera_settings_dialog_store.h"

namespace nx::vms::client::desktop {

class CameraSettingsResourceAccessWatcher::Private
{
public:
    CameraSettingsDialogStore* const store;
    QSet<QnVirtualCameraResourcePtr> cameras;

public:
    Qn::Permissions getPermissions(const QnVirtualCameraResourcePtr& camera)
    {
        if (const auto system = SystemContext::fromResource(camera))
        {
            if (const auto accessController = system->accessController())
                return accessController->permissions(camera);
        }

        return Qn::NoPermissions;
    }
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
    if (!NX_ASSERT(store && systemContext))
        return;

    connect(systemContext->accessController(),
        &core::AccessController::permissionsMaybeChanged,
        this,
        [this](const QnResourceList& resourcesHint)
        {
            if (resourcesHint.empty() || std::any_of(resourcesHint.cbegin(), resourcesHint.cend(),
                [this](const QnResourcePtr& resource)
                {
                    const auto camera = resource.objectCast<QnVirtualCameraResource>();
                    return camera && d->cameras.contains(camera);
                }))
            {
                d->store->setPermissions(singleCameraPermissions(), allCamerasEditable());
            }
        });

    if (const auto& action = menu->action(menu::ToggleScreenRecordingAction))
    {
        connect(action, &QAction::toggled, this,
            [this](bool checked){ d->store->setScreenRecordingOn(checked); });
    }
}

CameraSettingsResourceAccessWatcher::~CameraSettingsResourceAccessWatcher()
{
}

void CameraSettingsResourceAccessWatcher::setCameras(
    const QnVirtualCameraResourceList& cameras)
{
    d->cameras = nx::utils::toQSet(cameras);
}

Qn::Permissions CameraSettingsResourceAccessWatcher::singleCameraPermissions() const
{
    return d->cameras.size() == 1 ? d->getPermissions(*d->cameras.begin()) : Qn::NoPermissions;
}

bool CameraSettingsResourceAccessWatcher::allCamerasEditable() const
{
    return std::ranges::all_of(
        d->cameras,
        [this](const QnVirtualCameraResourcePtr& camera)
        {
            return d->getPermissions(camera).testFlags(
                Qn::ReadWriteSavePermission | Qn::WriteNamePermission);
        });
}

} // namespace nx::vms::client::desktop
