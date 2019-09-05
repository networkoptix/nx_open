// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>
#include <nx/sdk/result.h>

#include <nx/sdk/i_string.h>
#include <nx/sdk/i_plugin.h>

#include "i_engine.h"

namespace nx {
namespace sdk {
namespace analytics {

/**
 * The main interface for an Analytics Plugin instance.
 */
class IPlugin: public Interface<IPlugin, nx::sdk::IPlugin>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IPlugin"); }

    /**
     * Provides plugin manifest in JSON format.
     * @return JSON string in UTF-8.
     */
    protected: virtual void getManifest(Result<const IString*>* outResult) const = 0;
    public: Result<const IString*> manifest() const
    {
        Result<const IString*> result;
        getManifest(&result);
        return result;
    }

    /**
     * Creates a new instance of Analytics Engine.
     * @return Pointer to an object that implements the IEngine interface.
     */
    protected: virtual void doCreateEngine(Result<IEngine*>* outResult) = 0;
    public: Result<IEngine*> createEngine()
    {
        Result<IEngine*> result;
        doCreateEngine(&result);
        return result;
    }
};

} // namespace analytics
} // namespace sdk
} // namespace nx
