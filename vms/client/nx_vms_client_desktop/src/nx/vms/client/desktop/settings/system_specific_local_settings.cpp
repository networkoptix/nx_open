// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_specific_local_settings.h"

#include <nx/utils/log/log.h>
#include <nx/vms/client/desktop/system_context.h>

#include "system_specific_filesystem_backend.h"

namespace nx::vms::client::desktop {

using namespace nx::utils::property_storage;

SystemSpecificLocalSettings::SystemSpecificLocalSettings(SystemContext* systemContext):
    Storage(new SystemSpecificFileSystemBackend(systemContext)),
    SystemContextAware(systemContext)
{
    // Connect order ensures backend updates it's path earlier than settings are reloaded.
    connect(systemContext,
        &SystemContext::remoteIdChanged,
        this,
        [this](Uuid serverId)
        {
            if (!serverId.isNull())
            {
                NX_DEBUG(this, "Reload system-specific local settings.");
                load();
                save();
                generateDocumentation(
                    "Site-specific Settings", "Options actual for the currently connected site.");
            }
        });
}

void SystemSpecificLocalSettings::resetWarningsForAllSystems()
{
    const auto list = SystemSpecificFileSystemBackend::systemSpecificStoragePaths();
    NX_DEBUG(NX_SCOPE_TAG, "Reset warnings in %1 system specific storages", list.size());

    for (const auto& path: list)
    {
        std::unique_ptr<FileSystemBackend> backend(new FileSystemBackend(path));
        backend->writeValue("channelPartnerUserNotificationClosed", "false");
    }

    // Update the current instance. The changed() signals are sent inside.
    load();
}

} // namespace nx::vms::client::desktop
