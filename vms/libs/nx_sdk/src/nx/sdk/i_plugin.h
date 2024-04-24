// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

namespace nx::sdk {

class IUtilityProvider;

/**
 * The main interface that any VMS Plugin implements. The plugin's dynamic library should export
 * only an extern-C function with the name and prototype defined in this interface, which acts as a
 * getter/factory for a single object implementing this interface.
 *
 * The only object of this class is created by a Server on its start, and is destroyed (via
 * releaseRef()) on the Server shutdown.
 *
 * All methods are guaranteed to be called without overlapping even if from different threads (i.e.
 * with a guaranteed barrier between the calls), thus, no synchronization is required for the
 * implementation.
 *
 * ATTENTION: If the Plugins's dynamic library is linked to any dynamic libraries, including the
 * ones from the OS, consult @ref md_src_nx_sdk_dynamic_libraries to avoid potential issues.
 */
class IPlugin: public Interface<IPlugin>
{
public:
    static auto interfaceId()
    {
        return makeIdWithAlternative("nx::sdk::IPlugin",
            /* Planned future renaming. */ "nx::sdk::IIntegration");
    }

    /**
     * Prototype of a plugin entry point function for single-IPlugin Plugins.
     *
     * The Server calls this function only when the Plugin library does not export the
     * multi-IPlugin entry point function.
     */
    typedef IPlugin* (*EntryPointFunc)();

    /** Name of a plugin entry point function for single-IPlugin plugins. */
    static constexpr const char* kEntryPointFuncName = "createNxPlugin";

    /**
     * Prototype of a plugin entry point function for multi-IPlugin Plugins.
     *
     * The Server calls this function multiple times, passing sequential values starting from 0,
     * until the function returns null. Each non-null result is processed the same way as the
     * result of the single-IPlugin entry point function.
     *
     * If this function is exported from the Plugin library and returns at least one Plugin
     * instance, the single-IPlugin entry point function will not be called.
     */
    typedef IPlugin* (*MultiEntryPointFunc)(int instanceIndex);

    /** Name of a Plugin entry point function for multi-IPlugin Plugins. */
    static constexpr const char* kMultiEntryPointFuncName = "createNxPluginByIndex";

    /**
     * Provides an object which the plugin can use for calling back to access some data and
     * functionality provided by the process that uses the plugin.
     *
     * For details, see the documentation for IUtilityProvider.
     */
    virtual void setUtilityProvider(IUtilityProvider* utilityProvider) = 0;
};
using IPlugin0 = IPlugin;

} // namespace nx::sdk
