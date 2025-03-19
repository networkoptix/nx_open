// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_settings_global_settings_watcher.h"

#include <QtCore/QPointer>

#include <nx/utils/log/assert.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/client/desktop/system_context.h>

#include "../flux/camera_settings_dialog_store.h"

using namespace nx::vms::common;

namespace nx::vms::client::desktop {

CameraSettingsGlobalSettingsWatcher::CameraSettingsGlobalSettingsWatcher(
    CameraSettingsDialogStore* store,
    SystemContext* context,
    QObject* parent)
    :
    base_type(parent)
{
    NX_ASSERT(store);

    const auto updateCameraSettingsOptimizationEnabled =
        [context, store = QPointer<CameraSettingsDialogStore>(store)]()
        {
            if (!store)
                return;

            store->setSettingsOptimizationEnabled(
                context->globalSettings()->isCameraSettingsOptimizationEnabled());
        };

    updateCameraSettingsOptimizationEnabled();

    connect(context->globalSettings(), &SystemSettings::cameraSettingsOptimizationChanged,
        this, updateCameraSettingsOptimizationEnabled);
}

} // namespace nx::vms::client::desktop
