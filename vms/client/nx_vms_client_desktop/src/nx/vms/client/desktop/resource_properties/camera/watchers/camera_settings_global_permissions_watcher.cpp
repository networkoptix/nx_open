// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_settings_global_permissions_watcher.h"

#include <QtCore/QPointer>

#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/desktop/system_context.h>

#include "../flux/camera_settings_dialog_store.h"

namespace nx::vms::client::desktop {

CameraSettingsGlobalPermissionsWatcher::CameraSettingsGlobalPermissionsWatcher(
    CameraSettingsDialogStore* store,
    QObject* parent)
    :
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    NX_ASSERT(store);

    auto updateGlobalPermissions = nx::utils::guarded(store,
        [this, store](auto...)
        {
            store->setHasPowerUserPermissions(
                systemContext()->accessController()->hasPowerUserPermissions());

            store->setHasEventLogPermission(
                systemContext()->accessController()->hasGlobalPermissions(
                    GlobalPermission::viewLogs));
        });

    connect(systemContext()->accessController(),
        &nx::vms::client::core::AccessController::globalPermissionsChanged,
        this,
        updateGlobalPermissions);

    updateGlobalPermissions();
}

} // namespace nx::vms::client::desktop
