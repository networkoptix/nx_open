#pragma once

#include <nx/sdk/interface.h>
#include <nx/sdk/error.h>
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
     * @param outError Status of the operation; is set to noError before this call.
     * @return JSON string in UTF-8.
     */
    virtual const IString* manifest(Error* outError) const = 0;

    /**
     * Creates a new instance of Analytics Engine.
     * @param outError Status of the operation; is set to noError before this call.
     * @return Pointer to an object that implements DeviceAgent interface, or null in case of
     *     failure.
     */
    virtual IEngine* createEngine(Error* outError) = 0;

    /**
     * Name of the plugin dynamic library, without "lib" prefix and without extension.
     */
    virtual const char* name() const override = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
