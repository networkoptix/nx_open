// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

class QnClientSettings;
class QnClientShowOnceSettings;

namespace nx::vms::client::desktop {

class SharedMemoryManager;

/**
 * Syncronizes local client settings between running client instances using provided IPC.
 */
class IpcSettingsSynchronizer
{
public:
    static void setup(
        QnClientSettings* localSettings,
        QnClientShowOnceSettings* showOnceSettings,
        SharedMemoryManager* sharedMemoryManager);
};

} // namespace nx::vms::client::desktop
