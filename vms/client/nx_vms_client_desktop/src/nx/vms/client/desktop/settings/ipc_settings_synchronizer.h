// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::vms::client::desktop {

class LocalSettings;
class SharedMemoryManager;
class ShowOnceSettings;
class MessageBarSettings;

/**
 * Syncronizes local client settings between running client instances using provided IPC.
 */
class IpcSettingsSynchronizer
{
public:
    static void setup(
        LocalSettings* localSettings,
        ShowOnceSettings* showOnceSettings,
        MessageBarSettings* messageBarSettings,
        SharedMemoryManager* sharedMemoryManager);
};

} // namespace nx::vms::client::desktop
