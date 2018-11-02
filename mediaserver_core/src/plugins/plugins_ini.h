#pragma once

#include <nx/kit/ini_config.h>

// TODO: Rename and put into namespaces when lib common allows or when extracted from lib common.
/** Used in lib common for plugin framework. */
struct PluginsIniConfig: public nx::kit::IniConfig
{
    PluginsIniConfig(): nx::kit::IniConfig("plugins.ini") { reload(); }

    NX_INI_STRING("", disabledNxPlugins,
        "Comma-separated list of Nx plugins to skip in \"plugins\" dir,\n"
        "without extension and \"lib\" prefix. Can be \"*\", meaning \"all\".");

    NX_INI_STRING("", enabledNxPluginsOptional,
        "Comma-separated list of Nx plugins to load from \"plugins_optional\" dir,\n"
        "without extension and \"lib\" prefix. Can be \"*\", meaning \"all\".");

    NX_INI_STRING("", analyticsEngineSettingsPath,
        "Path to {plugin_name}.json: array of objects with name and value strings.\n"
        "Settings are passed to the Engine from the file only if this value is not empty,\n"
        "otherwise the standard database mechanism is used.");

    NX_INI_STRING("", analyticsDeviceAgentSettingsPath,
        "Path to {plugin_name}_device_agent_for_{device_id}.json:\n"
        "array of objects with name and value strings.\n"
        "Settings are passed to the DeviceAgent from the file only if this value is not empty,\n"
        "otherwise the standard database mechanism is used.");

    NX_INI_STRING("", analyticsManifestOutputPath,
        "Path (absolute or relative to .ini path) to dir for saving analytics plugin manifests.");
};

inline PluginsIniConfig& pluginsIni()
{
    static PluginsIniConfig ini;
    return ini;
}
