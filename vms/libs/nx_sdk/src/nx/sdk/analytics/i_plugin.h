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
    static auto interfaceId() { return InterfaceId("nx::sdk::analytics::IPlugin"); }

    /**
     * Provides plugin manifest in JSON format.
     *
     * @return JSON string in UTF-8.
     */
    virtual Result<const IString*> manifest() const = 0;

    /**
     * Creates a new instance of Analytics Engine.
     *
     * @return Pointer to an object that implements the IEngine interface.
     */
    virtual Result<IEngine*> createEngine() = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
