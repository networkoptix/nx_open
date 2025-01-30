// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/vms_server_plugins/analytics/sample/integration.h>

/**
 * Called by the Server to instantiate the Integration object.
 *
 * The Server requires the function to have C linkage, which leads to no C++ name mangling in the
 * export table of the plugin dynamic library, so that makes it possible to write plugins in any
 * language and compiler.
 *
 * NX_PLUGIN_API is the macro defined by CMake scripts for exporting the function.
 */
extern "C" NX_PLUGIN_API nx::sdk::IIntegration* createNxPlugin()
{
    // The object will be freed when the Server calls releaseRef().
    return new nx::vms_server_plugins::analytics::sample::Integration();
}
