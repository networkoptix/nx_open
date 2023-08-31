// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <nx/vms/api/data/analytics_data.h>
#include <nx/vms/api/data/saas_data.h>
#include <nx/vms/client/desktop/common/flux/abstract_flux_state.h>

namespace nx::vms::client::desktop {

struct NX_VMS_CLIENT_DESKTOP_API ServerSettingsDialogState: AbstractFluxState
{
    bool isOnline = false;

    struct BackupStoragesStatus
    {
        bool hasActiveBackupStorage = false;
        bool usesCloudBackupStorage = false;
        int enabledNonCloudStoragesCount = 0;
    };
    BackupStoragesStatus backupStoragesStatus;

    struct SaasProperties
    {
        nx::vms::api::SaasState saasState = nx::vms::api::SaasState::uninitialized;
        nx::vms::api::ServiceTypeStatus cloudStorageServicesStatus;
    };
    SaasProperties saasProperties;

    struct PluginsInformation
    {
        nx::vms::api::PluginInfoList modules;
        bool loading = false;
        int current = -1;
    };

    PluginsInformation plugins;

    std::optional<nx::vms::api::PluginInfo> currentPlugin() const
    {
        if (plugins.current < 0 || plugins.current > plugins.modules.size())
            return {};

        return plugins.modules[plugins.current];
    }
};

} // namespace nx::vms::client::desktop
