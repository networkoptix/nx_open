#pragma once

#include <memory>

#include <QtCore/QString>

#include <nx/utils/log/log.h>

class PluginManager; //< Used only for logging tag.

class PluginLoadingContextImpl; //< For Pimpl design pattern.

/**
 * Intended to perform required operations before loading a plugin (in constructor), and then
 * restore the original state of the process after calling plugin's entry point function (in
 * destructor).
 *
 * Currently, does the following, and only in Windows:
 *
 * If a plugin resides in its home dir, and .ini does not disable the feature, remembers the result
 * of WinAPI call GetDllDirectoryW(), and temporarily (to be restored in the destructor) sets
 * plugin's home dir with SetDllDirectoryW().
 */
class PluginLoadingContext final
{
public:
    /**
     * @param pluginHomeDir If empty, the plugin is assumed to reside in a dir with other plugins.
     */
    PluginLoadingContext(
        const PluginManager* pluginManager, QString pluginHomeDir, QString libName);

private:
    std::shared_ptr<PluginLoadingContextImpl> d; //< shared_ptr used due to a forward declaration.
};

