// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/i_string.h>
#include <nx/sdk/i_plugin.h>
#include <nx/sdk/i_utility_provider.h>

namespace nx {
namespace sdk {

/**
 * Provides plugin home directory which is used exclusively for the plugin's dynamic library and
 * possibly its dependencies and other plugin's resources; will be an empty string in case the
 * plugin is not installed in its dedicated directory but rather resides in a common directory with
 * other plugins.
 */
class IPluginHomeDirUtilityProvider:
    public Interface<IPluginHomeDirUtilityProvider, IUtilityProvider>
{
public:
    static auto interfaceId() { return InterfaceId("nx::sdk::IPluginHomeDirUtilityProvider"); }

    /**
     * @return Absolute path to the plugin's home directory, or an empty string if it is absent.
     */
    virtual const IString* homeDir(const nx::sdk::IPlugin* plugin) const = 0;
};

} // namespace sdk
} // namespace nx
