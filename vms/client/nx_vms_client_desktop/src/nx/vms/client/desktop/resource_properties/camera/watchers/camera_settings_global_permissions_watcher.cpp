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
    QnWorkbenchContextAware(parent, InitializationMode::instant)
{
    NX_ASSERT(store);

    auto updateGlobalPermissions =
        [this, store]()
        {
            store->setGlobalPermissions(accessController()->globalPermissions());
        };

    connect(accessController(), &QnWorkbenchAccessController::globalPermissionsChanged, this,
        updateGlobalPermissions);
    updateGlobalPermissions();
}

} // namespace nx::vms::client::desktop
