// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

namespace nx {
namespace sdk {

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
 */
class IPlugin: public Interface<IPlugin>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::IPlugin"); }

    /** Name of a plugin entry point function. */
    static constexpr const char* kEntryPointFuncName = "createNxPlugin";

    /** Prototype of a plugin entry point function. */
    typedef IPlugin* (*EntryPointFunc)();

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
