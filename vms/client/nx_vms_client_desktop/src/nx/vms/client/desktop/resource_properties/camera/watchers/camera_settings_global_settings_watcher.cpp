#include "camera_settings_global_settings_watcher.h"
#include "../redux/camera_settings_dialog_store.h"

#include <QtCore/QPointer>

#include <api/global_settings.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>

#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

namespace {

QnGlobalSettings* globalSettings()
{
    auto settings = qnClientCoreModule->commonModule()->globalSettings();
    NX_ASSERT(settings);
    return settings;
}

} // namespace

CameraSettingsGlobalSettingsWatcher::CameraSettingsGlobalSettingsWatcher(
    CameraSettingsDialogStore* store,
    QObject* parent)
    :
    base_type(parent)
{
    NX_ASSERT(store);

    const auto updateCameraSettingsOptimizationEnabled =
        [store = QPointer<CameraSettingsDialogStore>(store)]()
        {
            if (!store)
                return;

            store->setSettingsOptimizationEnabled(
                globalSettings()->isCameraSettingsOptimizationEnabled());
        };

    updateCameraSettingsOptimizationEnabled();

    connect(globalSettings(), &QnGlobalSettings::cameraSettingsOptimizationChanged,
        this, updateCameraSettingsOptimizationEnabled);
}

} // namespace nx::vms::client::desktop
