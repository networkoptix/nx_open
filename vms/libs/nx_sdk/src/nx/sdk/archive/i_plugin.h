// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>
#include <nx/sdk/i_plugin.h>
#include <nx/sdk/result.h>
#include <nx/sdk/i_string.h>

#include "i_device_manager.h"

namespace nx::sdk {
namespace archive {

/**
 * The Nx archive streaming/recording plugin SDK. It is conceived as an alternative to the Nx
 * Storage SDK for those cases when underlying implementation is stream based rather than file
 * system (in a broader sense of this notion) based. Plug-ins which implement this SDK are used
 * seamlessly by Nx VMS along with storage ones. 'Seamlessly' in this context means that from the
 * Nx User point of view video recording and playback should look the same whether the conventional
 * storages or stream based are used.
 */

/** Plugin main class. */
class IPlugin: public Interface<IPlugin, nx::sdk::IPlugin>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::archive::IPlugin"); }

    /** Provides an object for streaming/recording device management. */
    protected: virtual void doCreateDeviceManager(
        const char* url,
        const IDeviceManager::IDeviceManagerHandler* deviceManagerHandler,
        Result<IDeviceManager*>* outResult) = 0;

    /**
     * Plugin should create device managers only for urls which has it's id() as a scheme.
     */
    public: Result<IDeviceManager*> createDeviceManager(
        const char* url,
        const IDeviceManager::IDeviceManagerHandler* deviceManagerHandler)
    {
        Result<IDeviceManager*> result;
        doCreateDeviceManager(url, deviceManagerHandler, &result);
        return result;
    }

    /**
     * Plugin identificator. Should correspond to the scheme of the url accepted by
     * createDeviceManager() function. For example if id == my_cloud_e4f09, than url will look
     * like my_cloud_e4f09://login:password@10.20.101.31.
     */
    virtual const char* id() const = 0;

    protected: virtual void doQueryBookmarks(const char* filter, Result<IString*>* outResult) = 0;

    /**
    * TODO: Describe filter structure.
    */
    public: Result<IString*> queryBookmarks(const char* filter)
    {
        Result<IString*> result;
        doQueryBookmarks(filter, &result);
        return result;
    }

    protected: virtual void doQueryMotionTimePeriods(
        const char* filter,
        const char* timePeriodsLookupOptions,
        Result<IString*>* outResult) = 0;

    /**
    * TODO: Describe parameters.
    */
    public: Result<IString*> queryMotionTimePeriods(
        const char* filter,
        const char* timePeriodsLookupOptions)
    {
        Result<IString*> result;
        doQueryMotionTimePeriods(filter, timePeriodsLookupOptions, &result);
        return result;
    }

    protected: virtual void doQueryAnalytics(const char* filter, Result<IString*>* outResult) = 0;

    /**
     * Queries analytics data with the given filter.
     * @param filter TODO
     */
    public: Result<IString*> queryAnalytics(const char* filter)
    {
        Result<IString*> result;
        doQueryAnalytics(filter, &result);
        return result;
    }

    protected: virtual void doQueryAnalyticsTimePeriods(
        const char* filter,
        const char* timePeriodsLookupOptions,
        Result<IString*>* outResult) = 0;

    /**
     * Query periods of archive with analytics objects matching the given filter.
     * @param filter TODO
     * @param timePeriodsLookupOptions TODO
     */
    public: Result<IString*> queryAnalyticsTimePeriods(
        const char* filter,
        const char* timePeriodsLookupOptions)
    {
        Result<IString*> result;
        doQueryAnalyticsTimePeriods(filter, timePeriodsLookupOptions, &result);
        return result;
    }
};

} // namespace archive
} // namespace nx::sdk
