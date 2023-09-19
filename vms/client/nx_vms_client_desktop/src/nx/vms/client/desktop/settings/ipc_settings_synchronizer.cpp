// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ipc_settings_synchronizer.h"

#include <QtCore/QObject>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/settings/message_bar_settings.h>
#include <nx/vms/client/desktop/settings/show_once_settings.h>
#include <nx/vms/client/desktop/state/shared_memory_manager.h>

namespace nx::vms::client::desktop {

void IpcSettingsSynchronizer::setup(
    LocalSettings* localSettings,
    ShowOnceSettings* showOnceSettings,
    MessageBarSettings* messageBarSettings,
    SharedMemoryManager* sharedMemoryManager)
{
    NX_CRITICAL(localSettings);
    NX_CRITICAL(showOnceSettings);
    NX_CRITICAL(messageBarSettings);
    NX_CRITICAL(sharedMemoryManager);

    QObject::connect(sharedMemoryManager, &SharedMemoryManager::clientCommandRequested,
        localSettings,
        [localSettings](SharedMemoryData::Command command)
        {
            if (command == SharedMemoryData::Command::reloadSettings)
                localSettings->reload();
        });

    QObject::connect(sharedMemoryManager, &SharedMemoryManager::clientCommandRequested,
        showOnceSettings,
        [showOnceSettings](SharedMemoryData::Command command)
        {
            if (command == SharedMemoryData::Command::reloadSettings)
                showOnceSettings->reload();
        });

    QObject::connect(sharedMemoryManager, &SharedMemoryManager::clientCommandRequested,
        messageBarSettings,
        [messageBarSettings](SharedMemoryData::Command command)
        {
            if (command == SharedMemoryData::Command::reloadSettings)
                messageBarSettings->reload();
        });

    QObject::connect(localSettings, &LocalSettings::changed,
        sharedMemoryManager, &SharedMemoryManager::requestSettingsReload);
    QObject::connect(showOnceSettings, &ShowOnceSettings::changed,
        sharedMemoryManager, &SharedMemoryManager::requestSettingsReload);
    QObject::connect(showOnceSettings, &MessageBarSettings::changed,
        sharedMemoryManager, &SharedMemoryManager::requestSettingsReload);
}

} // namespace nx::vms::client::desktop
