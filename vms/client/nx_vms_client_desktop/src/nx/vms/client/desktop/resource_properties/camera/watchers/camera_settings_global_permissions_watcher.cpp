#include "camera_settings_global_permissions_watcher.h"
#include "../redux/camera_settings_dialog_store.h"

#include <QtCore/QPointer>

#include <ui/workbench/workbench_access_controller.h>

#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

CameraSettingsGlobalPermissionsWatcher::CameraSettingsGlobalPermissionsWatcher(
    CameraSettingsDialogStore* store,
    QObject* parent)
    :
    base_type(parent),
    QnWorkbenchContextAware(parent, InitializationMode::lazy)
{
    NX_ASSERT(store);

    connect(this, &CameraSettingsGlobalPermissionsWatcher::globalPermissionsChanged,
        store, &CameraSettingsDialogStore::setGlobalPermissions);
}

void CameraSettingsGlobalPermissionsWatcher::afterContextInitialized()
{
    connect(accessController(), &QnWorkbenchAccessController::globalPermissionsChanged, this,
        [this]() { emit globalPermissionsChanged(accessController()->globalPermissions(), {}); });
}

} // namespace nx::vms::client::desktop
