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

    static State setPluginModules(
        State state, const nx::vms::api::PluginModuleDataList& value);
    static State selectCurrentPlugin(State state, const QString& libraryName);
    static State setPluginsInformationLoading(State state, bool value);
};

} // namespace nx::vms::client::desktop
