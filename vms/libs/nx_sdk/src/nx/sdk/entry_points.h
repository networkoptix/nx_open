// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

/**@file
 * The plugin dynamic library should export only a number of extern-C functions with the name and
 * prototype defined in the SDK - we call such plugin functions "entry points". Those of them which
 * act like factories for main Integration objects are defined here.
 *
 * Note that there are additional entry point functions declared and implemented in
 * nx/sdk/helpers/lib_context.h. They give the Integration some useful capabilities, and require no
 * special efforts from the Integration creator besides encorporating that header and its .cpp.
 *
 * Note that there is an old-SDK entry point function defined in plugins/plugin_api.h - it can be
 * used for creating Integrations of old-SDK types (namely, Video Source and Storage).
 *
 * ATTENTION: If the plugin dynamic library is linked to any dynamic libraries, including the
 * ones from the OS, consult @ref md_src_nx_sdk_dynamic_libraries to avoid potential issues.
 */

#include <nx/sdk/i_integration.h>

namespace nx::sdk {

/**
 * Prototype of the entry point function for single-Integration plugins. Cannot be used for old-SDK
 * Integrations.
 *
 * The Server calls this function only when the plugin library does not export the
 * multi-Integration entry point function, or if the latter returns null for the index 0.
 */
extern "C" typedef IIntegration* createNxPlugin();
struct IntegrationEntryPoint
{
    static constexpr char kFuncName[] = "createNxPlugin";
    using Func = createNxPlugin*;
};

/**
 * Prototype of the entry point function for multi-Integration plugins.
 *
 * The Server calls this function multiple times, passing sequential values starting from 0,
 * until the function returns null. Each non-null result is processed similarly to the
 * single-Integration entry point functions (either the old-SDK or the new-SDK one), thus
 * it is possible to pack different IIntegration types into the same plugin dynamic library. If
 * an old-SDK Integration is returned, its pointer must be reinterpret-casted from
 * nxpl::PluginInterface - it is safe because nxpl::PluginInterface is ABI-compatible with
 * IIntegration.
 *
 * If this function is exported from the plugin library and returns at least one Integration
 * instance, the single-Integration entry point function will not be called.
 */
extern "C" typedef IIntegration* createNxPluginByIndex(int instanceIndex);
struct MultiIntegrationEntryPoint
{
    static constexpr char kFuncName[] = "createNxPluginByIndex";
    using Func = createNxPluginByIndex*;
};

} // namespace nx::sdk
