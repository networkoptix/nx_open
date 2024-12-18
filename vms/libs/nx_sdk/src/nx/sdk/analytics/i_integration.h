// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/i_integration.h>
#include <nx/sdk/i_string.h>
#include <nx/sdk/interface.h>
#include <nx/sdk/result.h>

#include "i_engine.h"

namespace nx::sdk::analytics {

/**
 * The main interface for an Analytics Plugin instance.
 */
class IIntegration: public Interface<IIntegration, nx::sdk::IIntegration0>
{
public:
    static auto interfaceId()
    {
        return makeIdWithAlternative("nx::sdk::analytics::IIntegration",
            /* Old name from 6.0. */ "nx::sdk::analytics::IPlugin");
    }

    /** Called by manifest() */
    protected: virtual void getManifest(Result<const IString*>* outResult) const = 0;
    /**
     * Provides Plugin Manifest in JSON format.
     *
     * See the manifest specification in @ref md_src_nx_sdk_analytics_manifests.
     *
     * @return JSON string in UTF-8.
     */
    public: Result<const IString*> manifest() const
    {
        Result<const IString*> result;
        getManifest(&result);
        return result;
    }

    /** Called by createEngine() */
    protected: virtual void doCreateEngine(Result<IEngine*>* outResult) = 0;
    /**
     * Creates a new instance of Analytics Engine.
     * @return Pointer to an object that implements the IEngine interface.
     */
    public: Result<IEngine*> createEngine()
    {
        Result<IEngine*> result;
        doCreateEngine(&result);
        return result;
    }
};
using IIntegration0 = IIntegration;

} // namespace nx::sdk::analytics
