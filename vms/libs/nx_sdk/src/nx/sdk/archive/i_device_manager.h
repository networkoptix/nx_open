// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>
#include <nx/sdk/result.h>
#include <nx/sdk/i_device_info.h>

#include <nx/sdk/i_plugin.h>
#include <nx/sdk/ptr.h>
#include <nx/sdk/result.h>
#include <nx/sdk/i_list.h>
#include <camera/camera_plugin.h>

#include "i_device.h"

namespace nx::sdk {
namespace archive {

enum class ArchiveAction
{
    add,
    remove,
};

/** Streaming/recording device management. */
class IDeviceManager: public Interface<IDeviceManager>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::archive::IDeviceManager"); }

    /**
     * A pointer to the object of this class is provided to the plugin when
     * IPlugin::createDeviceManager() is called. Implementation should inform Nx Mediaserver
     * about any external archive changes.
     */
    class IDeviceManagerHandler: public Interface<IDeviceManagerHandler>
    {
    public:
        static auto interfaceId()
        {
            return makeId("nx::sdk::archive::IDeviceManager::IDeviceManagerHandler");
        }

        virtual void onDeviceArchiveAltered(
            const char* pluginId,
            IDevice* device,
            int streamIndex,
            nxcip::TimePeriods* timePeriods,
            ArchiveAction archiveAction) const = 0;

    };

    /** Add a new device described by deviceInfo. If device exists, it should be returned. */
    protected: virtual void doAddDevice(
        Result<IDevice*>* outResult, const IDeviceInfo* deviceInfo) = 0;
    public: virtual Result<IDevice*> addDevice(const IDeviceInfo* deviceInfo)
    {
        Result<IDevice*> result;
        doAddDevice(&result, deviceInfo);
        return result;
    }

    /**
     * No IDeviceManagerHandler::onDeviceArchiveAltered() should be called before this function
     * has been called.
     */
    virtual void startNotifications() = 0;
};

} // namespace archive
} // namespace nx::sdk
