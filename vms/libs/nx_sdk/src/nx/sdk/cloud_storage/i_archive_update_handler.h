// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

#include "i_device_agent.h"
#include "i_time_periods.h"

namespace nx::sdk::cloud_storage {

enum class ArchiveAction
{
    add,
    remove,
};

/**
 * A pointer to the this class instance is provided to the engine when
 * IPlugin::createEngine() is called. The engine should use it to periodically inform Server
 * about any Archive changes.
 */
class IArchiveUpdateHandler: public Interface<IArchiveUpdateHandler>
{
public:
    static constexpr auto interfaceId()
    {
        return makeId("nx::sdk::archive::IArchiveUpdateHandler");
    }

    /**
     * Engine should call this periodically when/if something is changed on the backend:
     * some data was removed due to the retention policy, new data has been recorded,
     * new device appeared.
     */
    virtual void onArchiveUpdated(
        const char* engineId,
        IDeviceAgent* deviceAgent,
        int streamIndex,
        const ITimePeriods* timePeriods, //< Time periods that were added/removed to the backend archive.
        ArchiveAction archiveAction) const = 0;
};

} // namespace nx::sdk::cloud_storage