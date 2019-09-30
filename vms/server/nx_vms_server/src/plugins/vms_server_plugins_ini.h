#pragma once

#include <nx/kit/ini_config.h>

// TODO: Rename and put into namespaces when lib common allows or when extracted from lib common.
/** Used in lib common for plugin framework. */
struct PluginsIniConfig: public nx::kit::IniConfig
{
    PluginsIniConfig(): nx::kit::IniConfig("vms_server_plugins.ini") { reload(); }

    NX_INI_STRING("", disabledNxPlugins,
        "Comma-separated list of Nx plugins to skip in \"plugins\" dir,\n"
        "without extension and \"lib\" prefix. Can be \"*\", meaning \"all\".\n"
        "Example: you don't want the server to load the USB Camera Plugin and Generic\n"
        "Multicast Plugin (the library names on Linux are libusb_camera_plugin.so and\n"
        "libgeneric_multicast_plugin.so, on Windows the libraries are\n"
        "usb_camera_plugin.dll and generic_multicast_plugin.dll.\n"
        "Set the value of this setting to \"usb_camera_plugin,generic_multicast_plugin\"");

    NX_INI_STRING("", enabledNxPluginsOptional,
        "Comma-separated list of Nx plugins to load from \"plugins_optional\" dir,\n"
        "without extension and \"lib\" prefix. Can be \"*\", meaning \"all\".\n"
        "The syntax is identical to disabledNxPlugins setting. The difference is that\n"
        "this setting enables the plugins that are disabled by default, and the\n"
        "disabledNxPlugins setting disables the plugins that are enabled by default.");

    NX_INI_FLAG(0, disablePluginLinkedDllLookup,
        "In Windows, for Plugins residing in a dedicated plugin folder, do not set DLL search\n"
        "path to that folder via SetDllDirectoryW().");

    NX_INI_STRING("", analyticsManifestSubstitutePath,
        "Path (absolute or relative to .ini dir) to dir with manifests that will be used instead\n"
        "of real manifests from Plugins, Engines and DeviceAgents. The filename pattern is the\n"
        "same as of manifests generated in analyticsManifestOutputPath.");

    NX_INI_STRING("", analyticsManifestOutputPath,
        "Path (absolute or relative to .ini dir) to existing dir for saving analytics plugin\n"
        "manifests.");

    NX_INI_STRING("", analyticsSettingsSubstitutePath,
        "Path (absolute or relative to .ini dir) to dir with settings for Engines and\n"
        "DeviceAgents, that will be used instead of the values of settings from the Server\n"
        "database, in case the properly named file is present in this directory. Such files\n"
        "should be JSON with an array of objects with setting name and value strings.\n"
        "File name format for Engine (all Engines of the plugin match):\n"
        "    {plugin_name}_engine_settings.json\n"
        "File name format for Engine (only the specified Engine matches):\n"
        "    {plugin_name}_engine_{engine_id}_settings.json\n"
        "File name format for DeviceAgent:\n"
        "    {plugin_name}_device_{device_id}_settings.json\n"
        "Here {plugin_name} is the library name without extension and \"lib\" prefix.");

    NX_INI_STRING("", analyticsSettingsOutputPath,
        "Path (absolute or relative to .ini dir) to existing dir for saving settings that the\n"
        "Server sends to an analytics plugin.");

    NX_INI_FLAG(0, tryAllLibsInPluginDir,
        "Attempt to load each dynamic library from each plugin directory instead of only the one\n"
        "with the plugin_name equal to the directory name.");

    NX_INI_FLAG(0, enableRefCountableRegistry,
        "Turn on a debugging mechanism that tracks ref-countable object creation and deletion to\n"
        "detect leaks and double-frees. An assertion will fail if a discrepancy is detected.\n"
        "The Server and each Plugin have their own instance of such registry, tracking objects\n"
        "created/destroyed in the respective module.");

    NX_INI_FLAG(0, verboseRefCountableRegistry,
        "Turn on verbose output of the ref-countable registry (if it is enabled): log each\n"
        "ref-countable object creating and deleting.");

    NX_INI_FLAG(0, useServerLogForRefCountableRegistry,
        "Whether the ref-countable registry should write its logs to the regular Server log with\n"
        "the level INFO, instead of stderr. Also the type of assertions used by the registry\n"
        "depends on this value: NX_KIT_ASSERT() for stderr, NX_ASSERT() for the Server log.");

    NX_INI_FLAG(0, enableStrictManifestValidationMode,
        "If set then any error in a manifest leads to the Plugin/Engine/DeviceAgent rejection.");
};

inline PluginsIniConfig& pluginsIni()
{
    static PluginsIniConfig ini;
    return ini;
}
