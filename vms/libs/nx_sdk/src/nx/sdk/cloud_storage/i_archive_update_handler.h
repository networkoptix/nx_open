// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/i_list.h>
#include <nx/sdk/interface.h>

#include "i_device_agent.h"
#include "i_time_periods.h"

namespace nx::sdk::cloud_storage {

/**
 * Added and removed time period lists per one stream.
 */
class IIndexArchive: public Interface<IIndexArchive>
{
public:
    virtual const ITimePeriods* addedTimePeriods() const = 0;
    virtual const ITimePeriods* removedTimePeriods() const = 0;
    virtual int streamIndex() const = 0;
};

/**
 * A Device Archive consisting of the list of the stream Archives.
 */
class IDeviceArchive: public Interface<IDeviceArchive>
{
public:
    virtual IList<IIndexArchive>* indexArchive() = 0;
    virtual IDeviceAgent* deviceAgent() const = 0;
};

/**
 * A pointer to the this class instance is provided to the engine when
 * IPlugin::createEngine() is called. The engine should use it to periodically inform Server
 * about any Archive changes.
 */
class IArchiveUpdateHandler: public Interface<IArchiveUpdateHandler>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::archive::IArchiveUpdateHandler"); }

    /**
     * Engine should call this periodically to check if something is changed on the backend.
     * I.e. some data was removed due to the retention policy, new data has been recorded,
     * new device appeared.
     * The deviceArchive may be destroyed by the plugin right after the onArchiveUpdated function
     * has returned.
     */
    virtual void onArchiveUpdated(
        const char* engineId,
        nx::sdk::ErrorCode errorCode,
        const IList<IDeviceArchive>* deviceArchive) const = 0;
};

} // namespace nx::sdk::cloud_storage
