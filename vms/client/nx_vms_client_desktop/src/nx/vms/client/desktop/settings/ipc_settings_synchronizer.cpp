// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ipc_settings_synchronizer.h"

#include <QtCore/QObject>

#include <client/client_settings.h>
#include <client/client_show_once_settings.h>
#include <nx/vms/client/desktop/state/shared_memory_manager.h>

#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

void IpcSettingsSynchronizer::setup(
    QnClientSettings* localSettings,
    QnClientShowOnceSettings* showOnceSettings,
    SharedMemoryManager* sharedMemoryManager)
{
    NX_CRITICAL(localSettings);
    NX_CRITICAL(showOnceSettings);
    NX_CRITICAL(sharedMemoryManager);

    QObject::connect(sharedMemoryManager, &SharedMemoryManager::clientCommandRequested,
        localSettings,
        [localSettings](SharedMemoryData::Command command)
        {
            if (command == SharedMemoryData::Command::reloadSettings)
                localSettings->load();
        });

    QObject::connect(sharedMemoryManager, &SharedMemoryManager::clientCommandRequested,
        showOnceSettings,
        [showOnceSettings](SharedMemoryData::Command command)
        {
            if (command == SharedMemoryData::Command::reloadSettings)
                showOnceSettings->sync();
        });

    QObject::connect(localSettings, &QnClientSettings::saved,
        sharedMemoryManager, &SharedMemoryManager::requestSettingsReload);
    QObject::connect(showOnceSettings, &nx::settings::ShowOnce::changed,
        sharedMemoryManager, &SharedMemoryManager::requestSettingsReload);
}

} // namespace nx::vms::client::desktop
