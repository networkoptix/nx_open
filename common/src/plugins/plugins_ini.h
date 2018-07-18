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

    NX_INI_STRING("", metadataPluginSettingsPath,
        "Path to {plugin_name}.json: array of objects with name and value strings.\n"
        "Empty means .ini path.");

    NX_INI_STRING("", metadataPluginCameraManagerSettingsPath,
        "Path to {plugin_name}_camera_manager.json: array of objects with name and value strings.\n"
        "Empty means .ini path.");

    NX_INI_STRING("", metadataPluginManifestOutputPath,
        "Path (absolute or relative to .ini path) to dir for saving metadata plugin manifests.");
};

inline PluginsIniConfig& pluginsIni()
{
    static PluginsIniConfig ini;
    return ini;
}
