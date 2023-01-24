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
        [this]
        {
            NX_DEBUG(this, "Reload system-specific local settings.");
            load();
        });
}

} // namespace nx::vms::client::desktop
