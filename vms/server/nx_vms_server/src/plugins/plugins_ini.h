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
        "Path (absolute or relative to .ini dir) to {plugin_name}_engine_settings.json: array\n"
        "of objects with name and value strings. Settings are passed to the Engine from the file\n"
        "only if this value is not empty and the file is present in the filesystem. Otherwise\n"
        "the setttings from the database are used.");

    NX_INI_STRING("", analyticsDeviceAgentSettingsPath,
        "Path (absolute or relative to .ini dir) to\n"
        "{plugin_name}_device_{device_unique_id}_settings.json: array of objects with name and\n"
        "value strings. Settings are passed to the DeviceAgent from the file only if this value\n"
        "is not empty and the file is present in the filesystem. Otherwise the settings from the\n"
        "database are used.");

    NX_INI_STRING("", analyticsManifestOutputPath,
        "Path (absolute or relative to .ini dir) to dir for saving analytics plugin manifests.");

    NX_INI_STRING("", analyticsSettingsOutputPath,
        "Path (absolute or relative to .ini dir) to dir for saving settings that the Server\n"
        "sends to an analytics plugin.");

    NX_INI_FLAG(false, tryAllLibsInPluginDir,
        "Attempt to load each dynamic library from each plugin directory instead of only the one\n"
        "with the plugin_name equal to the directory name.");

    NX_INI_FLAG(false, useRefCountableRegistry,
        "Turn on a debugging mechanism that tracks ref-countable object creation and deletion to\n"
        "detect leaks and double-frees. An assertion will fail if a discrepancy is detected.\n"
        "The Server and each Plugin have their own instance of such registry, tracking objects\n"
        "created/destroyed in the respective module.");

    NX_INI_FLAG(false, verboseRefCountableRegistry,
        "Turn on verbose output of the ref-countable registry (if it is enabled).");
};

inline PluginsIniConfig& pluginsIni()
{
    static PluginsIniConfig ini;
    return ini;
}
