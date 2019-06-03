#include "debug_helpers.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>

#include <core/resource/camera_resource.h>
#include <plugins/vms_server_plugins_ini.h>

#include <nx/utils/log/log.h>
#include <nx/utils/log/log_level.h>
#include <nx/utils/file_system.h>
#include <nx/utils/debug_helpers/debug_helpers.h>

#include <nx/vms/server/resource/analytics_engine_resource.h>
#include <nx/vms/server/resource/analytics_plugin_resource.h>

#include <nx/vms/server/sdk_support/utils.h>

#include <nx/sdk/helpers/ptr.h>

namespace nx::vms::server::analytics::debug_helpers {

namespace {

static const QString kSettingsFilenamePostfix("_settings.json");

// Examples of filenames used to save/load manifests:
// DeviceAgent: stub_analytics_plugin_device_a25c01be-f7dc-4600-8b8e-915bf4c0a688_manifest.json
// Engine: stub_analytics_plugin_engine_manifest.json
// Plugin: stub_analytics_plugin_manifest.json
//
// Examples of filenames used to save/load settings:
// DeviceAgent: stub_analytics_plugin_device_a25c01be-f7dc-4600-8b8e-915bf4c0a688_settings.json
// Engine: stub_analytics_plugin_engine_settings.json

static QString pluginLibName(
    const nx::sdk::analytics::IPlugin* plugin)
{
    if (!NX_ASSERT(plugin))
        return QString();

    const char* const pluginName = plugin->name();
    if (!pluginName)
        NX_ERROR(NX_SCOPE_TAG, "Plugin name is null Plugin instance %1", plugin);
    if (pluginName[0] == '\0')
        NX_ERROR(NX_SCOPE_TAG, "Plugin name is empty for Plugin instance %1", plugin);
    return pluginName;
}

static QString pluginLibName(
    const nx::vms::server::resource::AnalyticsPluginResourcePtr& pluginResource)
{
    if (!NX_ASSERT(pluginResource))
        return QString();

    const auto plugin = pluginResource->sdkPlugin();
    if (!NX_ASSERT(plugin, "Unable to access SDK's Plugin for Plugin Resource %1", pluginResource))
        return QString();

    const char* const pluginName = plugin->name();
    if (!pluginName)
        NX_ERROR(NX_SCOPE_TAG, "Plugin name is null for Plugin Resource %1", pluginResource);
    if (pluginName[0] == '\0')
        NX_ERROR(NX_SCOPE_TAG, "Plugin name is empty for Plugin Resource %1", pluginResource);
    return pluginName;
}

static QString pluginLibName(
    const nx::vms::server::resource::AnalyticsEngineResourcePtr& engineResource)
{
    const auto pluginResource =
        engineResource->plugin().dynamicCast<nx::vms::server::resource::AnalyticsPluginResource>();

    if (!NX_ASSERT(pluginResource))
        return QString();

    return pluginLibName(pluginResource);
}

static QString nameOfFileToDumpOrLoadDataForDevice(
    const QnVirtualCameraResourcePtr& deviceResource,
    const nx::vms::server::resource::AnalyticsEngineResourcePtr& engineResource,
    const QString& postfix)
{
    if (!NX_ASSERT(engineResource))
        return QString();

    if (!NX_ASSERT(deviceResource))
        return QString();

    const auto libName = pluginLibName(engineResource);
    if (!NX_ASSERT(!libName.isEmpty()))
        return QString();

    return libName + "_device_" + deviceResource->getId().toSimpleString() + postfix;
}

static QString nameOfFileToDumpOrLoadDataForEngine(
    const nx::vms::server::resource::AnalyticsEngineResourcePtr& engineResource,
    const QString& postfix,
    bool useEngineId)
{
    if (!NX_ASSERT(engineResource))
        return QString();

    const auto libName = pluginLibName(engineResource);
    if (!NX_ASSERT(!libName.isEmpty()))
        return QString();

    QString result = libName + "_engine";
    if (useEngineId)
        result += "_" + engineResource->getId().toSimpleString();

    result += postfix;
    return result;
}

/** @param settingsFilename If empty, the call does nothing - an assertion has already failed. */
static nx::sdk::Ptr<nx::sdk::IStringMap> doLoadEngineSettingsFromFile(
    const char* fileDir, const QString& settingsFilename)
{
    if (settingsFilename.isEmpty())
        return nullptr;

    if (!NX_ASSERT(fileDir) || !NX_ASSERT(fileDir[0]))
        return nullptr;

    const QDir dir(nx::utils::debug_helpers::debugFilesDirectoryPath(fileDir));

    return loadSettingsFromFile("Engine settings", dir.absoluteFilePath(settingsFilename));
}

} // namespace

nx::sdk::Ptr<nx::sdk::IStringMap> loadSettingsFromFile(
    const QString& fileDescription,
    const QString& filename)
{
    using nx::utils::log::Level;
    const auto log = //< Can be used to return empty settings: return log(...)
        [&](Level level, const QString& message)
        {
            NX_UTILS_LOG(level, NX_SCOPE_TAG, "Analytics %1 settings: %2: [%3]",
                fileDescription, message, filename);
            return nullptr;
        };

    const QString settingsJson = loadStringFromFile(filename, log);
    if (settingsJson.isEmpty())
        return log(Level::error, "Unable to read from file");

    const auto settings = sdk_support::toIStringMap(settingsJson);
    if (!settings)
        return log(Level::error, "Invalid JSON in file");

    log(Level::warning, "Loaded from file");
    return settings;
}

nx::sdk::Ptr<nx::sdk::IStringMap> loadDeviceAgentSettingsFromFile(
    const QnVirtualCameraResourcePtr& device,
    const nx::vms::server::resource::AnalyticsEngineResourcePtr& engine,
    const char* fileDir)
{
    const auto settingsFilename = nameOfFileToDumpOrLoadDataForDevice(
        device, engine, kSettingsFilenamePostfix);
    if (settingsFilename.isEmpty())
        return nullptr;

    if (!NX_ASSERT(fileDir) || !NX_ASSERT(fileDir))
        return nullptr;

    const QDir dir(nx::utils::debug_helpers::debugFilesDirectoryPath(fileDir));

    return loadSettingsFromFile("DeviceAgent settings", dir.absoluteFilePath(settingsFilename));
}

nx::sdk::Ptr<nx::sdk::IStringMap> loadEngineSettingsFromFile(
    const nx::vms::server::resource::AnalyticsEngineResourcePtr& engine, const char* fileDir)
{
    const QString specificFileName = nameOfFileToDumpOrLoadDataForEngine(
        engine, kSettingsFilenamePostfix, /*useEngineId*/ true);

    NX_WARNING(NX_SCOPE_TAG, "Trying to load Engine %1 settings from the Engine-specific file %2",
        engine, specificFileName);

    const nx::sdk::Ptr<nx::sdk::IStringMap> settings =
        doLoadEngineSettingsFromFile(fileDir, specificFileName);
    if (settings)
        return settings;

    const QString genericFileName = nameOfFileToDumpOrLoadDataForEngine(
        engine, kSettingsFilenamePostfix, /*useEngineId*/ false);

    NX_WARNING(NX_SCOPE_TAG, "Trying to load Engine %1 settings from the generic file %2",
        engine, genericFileName);

    return doLoadEngineSettingsFromFile(fileDir, genericFileName);
}

QString nameOfFileToDumpOrLoadData(
    const QnVirtualCameraResourcePtr& device,
    const nx::vms::server::resource::AnalyticsEngineResourcePtr& engine,
    const nx::vms::server::resource::AnalyticsPluginResourcePtr& plugin,
    const QString& postfix)
{
    NX_ASSERT(!postfix.isEmpty());

    if (device && engine) //< Filename for something related to a DeviceAgent.
        return nameOfFileToDumpOrLoadDataForDevice(device, engine, postfix);
    else if (engine) //< Filename for something related to an Engine.
        return nameOfFileToDumpOrLoadDataForEngine(engine, postfix, /*useEngineId*/ true);
    else if (plugin) //< Filename for something related to a Plugin.
        return pluginLibName(plugin) + postfix;

    NX_ASSERT(false, "Incorrect parameters");
    return QString();
}

QString nameOfFileToDumpOrLoadData(
    const nx::sdk::analytics::IPlugin* plugin,
    const QString& postfix)
{
    NX_ASSERT(!postfix.isEmpty());

    if (!NX_ASSERT(plugin))
        return QString();

    return pluginLibName(plugin) + postfix;
}

void dumpStringToFile(
    const nx::utils::log::Tag& logTag,
    const QString& stringToDump,
    const QString& directoryPath,
    const QString& filename)
{
    using nx::utils::log::Level;
    auto log = //< Can be used to return after logging: return log(...).
        [&logTag, &filename](Level level, const QString& message)
        {
            NX_UTILS_LOG(level, logTag, "Dumping to file: %1: [%2]", message, filename);
        };

    log(Level::info, "Saving to file");

    QDir dir(nx::utils::debug_helpers::debugFilesDirectoryPath(directoryPath));
    if (!nx::utils::file_system::ensureDir(dir))
        return log(Level::error, "Unable to create directory for file");

    QFile f(dir.absoluteFilePath(filename));
    if (!f.open(QFile::WriteOnly))
        return log(Level::error, "Unable to (re)create file");

    if (f.write(stringToDump.toUtf8()) < 0)
        return log(Level::error, "Unable to write to file");

    if (!stringToDump.isEmpty() && stringToDump.back() != '\n')
    {
        if (f.write("\n") < 0)
            return log(Level::error, "Unable to write trailing `\n` to file");
    }
}

QString loadStringFromFile(
    const QString& filename,
    std::function<void(nx::utils::log::Level, const QString& message)> logger)
{
    using nx::utils::log::Level;
    if (filename.isEmpty())
    {
        logger(Level::error, "File name is empty");
        return QString();
    }

    if (!QFileInfo::exists(filename))
    {
        logger(Level::info, "File does not exist");
        return QString();
    }

    logger(Level::info, "Loading from file");

    QFile f(filename);
    if (!f.open(QFile::ReadOnly))
    {
        logger(Level::error, "Unable to open file");
        return QString();
    }

    return f.readAll();
}

} // namespace nx::vms::server::analytics::debug_helpers
