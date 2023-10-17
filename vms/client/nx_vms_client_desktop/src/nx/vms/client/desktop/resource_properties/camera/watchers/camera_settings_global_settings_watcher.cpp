// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_settings_global_settings_watcher.h"

#include <QtCore/QPointer>

#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>

#include "../flux/camera_settings_dialog_store.h"

using namespace nx::vms::common;

namespace nx::vms::client::desktop {

namespace {

SystemSettings* globalSettings()
{
    auto settings = qnClientCoreModule->globalSettings();
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

    connect(globalSettings(), &SystemSettings::cameraSettingsOptimizationChanged,
        this, updateCameraSettingsOptimizationEnabled);
}

} // namespace nx::vms::client::desktop
