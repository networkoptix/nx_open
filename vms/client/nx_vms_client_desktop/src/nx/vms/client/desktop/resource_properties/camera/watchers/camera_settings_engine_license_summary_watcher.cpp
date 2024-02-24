// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_settings_engine_license_summary_watcher.h"

#include <nx/vms/client/desktop/common/utils/engine_license_summary_provider.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/saas/saas_service_usage_helper.h>

#include "../flux/camera_settings_dialog_store.h"

namespace nx::vms::client::desktop {

CameraSettingsEngineLicenseWatcher::CameraSettingsEngineLicenseWatcher(
    CameraSettingsDialogStore* store, SystemContext* context, QObject* parent)
    :
    QObject(parent),
    SystemContextAware(context)
{
    connect(
        systemContext()->saasServiceManager(),
        &common::saas::ServiceManager::dataChanged,
        this,
        [this, store]()
        {
            if (!store->state().isSingleCamera())
                return;

            EngineLicenseSummaryProvider engineLicenseSummaryProvider{systemContext()};
            store->handleOverusedEngines(engineLicenseSummaryProvider.overusedEngines(
                store->resource(), store->state().analytics.userEnabledEngines.get()));
        });
}

} // namespace nx::vms::client::desktop
