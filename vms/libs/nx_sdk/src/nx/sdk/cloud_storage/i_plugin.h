// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/i_plugin.h>
#include <nx/sdk/i_string.h>
#include <nx/sdk/interface.h>
#include <nx/sdk/result.h>

#include "i_archive_update_handler.h"
#include "i_engine.h"

namespace nx::sdk::cloud_storage {

/**
 * The main interface of a Cloud Storage Plugin instance.
 */
class IPlugin: public Interface<IPlugin, nx::sdk::IPlugin>
{
public:
    static constexpr auto interfaceId() { return makeId("nx::sdk::archive::IPlugin"); }
    static auto alternativeInterfaceId() { return makeId("nx::sdk::archive::IIntegration"); }

    protected: virtual void getManifest(Result<const IString*>* outResult) const = 0;
    /**
     * Provides a JSON Manifest for this Plugin instance. See the example of such Manifest in
     * stub_cloud_storage_plugin.
     */
    public: Result<const IString*> manifest() const
    {
        Result<const IString*> result;
        getManifest(&result);
        return result;
    }

    protected: virtual void doObtainEngine(const char* url,
        const IArchiveUpdateHandler* archiveUpdateHandler,
        Result<IEngine*>* outResult) = 0;

    /**
     * Obtain engine corresponding to the backed url. Now Server won't ask the same plugin
     * for multiple engines, but potentially it may be implemented. Plugin should store
     * engine internally and not recreate it if this function is called more than once
     * with the same url.
     */
    public: Result<IEngine*> obtainEngine(
        const char* url, const IArchiveUpdateHandler* archiveUpdateHandler)
    {
        Result<IEngine*> result;
        doObtainEngine(url, archiveUpdateHandler, &result);
        return result;
    }
};

} // namespace nx::sdk::cloud_storage
