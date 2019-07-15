// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>
#include <nx/sdk/result.h>

#include <nx/sdk/error_code.h>
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
     * @return Result containing JSON string in UTF-8 in case of success of error information in
     *     case of failure.
     */
    virtual Result<const IString*> manifest() const = 0;

    /**
     * Creates a new instance of Analytics Engine.
     *
     * @return Result containing a pointer to an object that implements the IEngine interface, or
     *     error information in case of failure.
     */
    virtual Result<IEngine*> createEngine() = 0;

    /**
     * Name of the plugin dynamic library, without "lib" prefix and without extension.
     */
    virtual const char* name() const override = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
