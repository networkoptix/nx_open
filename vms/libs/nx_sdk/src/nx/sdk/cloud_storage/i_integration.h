// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/i_integration.h>
#include <nx/sdk/i_string.h>
#include <nx/sdk/interface.h>
#include <nx/sdk/result.h>

#include "i_async_operation_handler.h"
#include "i_engine.h"

namespace nx::sdk::cloud_storage {

/**
 * The main interface of a Cloud Storage Plugin instance.
 */
class IIntegration: public Interface<IIntegration, nx::sdk::IIntegration>
{
public:
    static auto interfaceId()
    {
        return makeIdWithAlternative("nx::sdk::archive::IIntegration",
            /* Old name from 6.0. */ "nx::sdk::archive::IPlugin");
    }

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
        const IAsyncOperationHandler* asyncOperationHandler,
        Result<IEngine*>* outResult) = 0;

    /**
     * Obtain engine corresponding to the backed url. Now Server won't ask the same plugin
     * for multiple engines, but potentially it may be implemented. Plugin should store
     * engine internally and not recreate it if this function is called more than once
     * with the same url.
     */
    public: Result<IEngine*> obtainEngine(
        const char* url, const IAsyncOperationHandler* asyncOperationHandler)
    {
        Result<IEngine*> result;
        doObtainEngine(url, asyncOperationHandler, &result);
        return result;
    }
};

} // namespace nx::sdk::cloud_storage
