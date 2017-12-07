#pragma once

#include <nx/kit/ini_config.h>

// TODO: Rename and put into namespaces when lib common allows or when extracted from lib common.
/** Used in lib common for plugin framework. */
struct PluginsIniConfig: public nx::kit::IniConfig
{
    PluginsIniConfig(): nx::kit::IniConfig("plugins.ini") { reload(); }

    NX_INI_STRING("", disabledNxPlugins,
        "Comma-separated list of Nx plugins to skip, without extension or 'lib' prefix.");
};

inline PluginsIniConfig& pluginsIni()
{
    static PluginsIniConfig ini;
    return ini;
}
