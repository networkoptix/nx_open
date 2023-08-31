// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>

#include "server_settings_dialog_state.h"

namespace nx::vms::client::desktop {

class NX_VMS_CLIENT_DESKTOP_API ServerSettingsDialogStateReducer
{
public:
    using State = ServerSettingsDialogState;

    static State loadServer(State state, const QnMediaServerResourcePtr& server);

    static State setOnline(State state, bool value);

    static State setBackupStoragesStatus(
        State state,
        ServerSettingsDialogState::BackupStoragesStatus storagesStatus);

    static State setSaasState(
        State state,
        nx::vms::api::SaasState saasState);

    static State setSaasCloudStorageServicesStatus(
        State state,
        nx::vms::api::ServiceTypeStatus servicesStatus);

    static State setPluginModules(
        State state, const nx::vms::api::PluginInfoList& value);
    static State selectCurrentPlugin(State state, const QString& libraryFilename);
    static State setPluginsInformationLoading(State state, bool value);
};

} // namespace nx::vms::client::desktop
