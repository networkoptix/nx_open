// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_settings_global_permissions_watcher.h"

#include <QtCore/QPointer>

#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/assert.h>
#include <ui/workbench/workbench_access_controller.h>

#include "../flux/camera_settings_dialog_store.h"

namespace nx::vms::client::desktop {

CameraSettingsGlobalPermissionsWatcher::CameraSettingsGlobalPermissionsWatcher(
    CameraSettingsDialogStore* store,
    QObject* parent)
    :
    base_type(parent),
    QnWorkbenchContextAware(parent, InitializationMode::instant)
{
    NX_ASSERT(store);

    auto updateGlobalPermissions = nx::utils::guarded(store,
        [this, store]()
        {
            store->setGlobalPermissions(accessController()->globalPermissions());
        });

    connect(accessController(), &QnWorkbenchAccessController::globalPermissionsChanged, this,
        updateGlobalPermissions);
    updateGlobalPermissions();
}

} // namespace nx::vms::client::desktop
