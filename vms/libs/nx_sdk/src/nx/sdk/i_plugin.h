#pragma once

#include <nx/sdk/interface.h>
#include <nx/sdk/i_utility_provider.h>

namespace nx {
namespace sdk {

/**
 * The main interface that any VMS Plugin implements. The plugin's dynamic library should export
 * only an extern-C function with the name and prototype defined in this interface, which acts as a
 * getter/factory for a single object implementing this interface.
 *
 * The only object of this class is created by a Server on its start, and is destroyed (via
 * releaseRef()) on the Server shutdown.
 */
class IPlugin: public Interface<IPlugin>
{
public:
    static auto interfaceId() { return InterfaceId("nx::sdk::IPlugin"); }

    /** Name of a plugin entry point function. */
    static constexpr const char* kEntryPointFuncName = "createNxPlugin";

    /** Prototype of a plugin entry point function. */
    typedef IPlugin* (*EntryPointFunc)();

    /** Name of the plugin, used for information purpose only. */
    virtual const char* name() const = 0;

    /**
     * Provides an object which the plugin can use for calling back to access some data and
     * functionality provided by the process that uses the plugin.
     *
     * For details, see the documentation for IUtilityProvider.
     */
    virtual void setUtilityProvider(IUtilityProvider* utilityProvider) = 0;
};

} // namespace sdk
} // namespace nx
