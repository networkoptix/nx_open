#include "debug_helpers.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>

#include <core/resource/camera_resource.h>
#include <plugins/plugins_ini.h>

#include <nx/utils/log/log.h>
#include <nx/utils/log/log_level.h>
#include <nx/utils/file_system.h>

#include <nx/vms/server/resource/analytics_engine_resource.h>
#include <nx/vms/server/resource/analytics_plugin_resource.h>

#include <nx/vms/server/sdk_support/utils.h>

#include <nx/sdk/common/ptr.h>

namespace nx::vms::server::analytics::debug_helpers {

namespace {

static nx::utils::log::Tag kLogTag(QString("AnalyticsDebugHelpers"));
static const QString kSettingsFilenamePostfix("_settings.json");

// Examples of filenames used to save manifests to:
// DeviceAgent: stub_analytics_plugin_device_a25c01be-f7dc-4600-8b8e-915bf4c0a688_manifest.json
// Engine: stub_analytics_plugin_engine_manifest.json
// Plugin: stub_analytics_plugin_manifest.json
//
// Examples of filenames used to load settings from:
// DeviceAgent: stub_analytics_plugin_device_a25c01be-f7dc-4600-8b8e-915bf4c0a688_settings.json
// Engine: stub_analytics_plugin_engine_settings.json

QString pluginLibName(const nx::sdk::analytics::IPlugin* plugin)
{
    if (!NX_ASSERT(plugin))
        return QString();

    return plugin->name();
}

QString pluginLibName(const nx::vms::server::resource::AnalyticsPluginResourcePtr& pluginResource)
{
    if (const auto plugin = pluginResource->sdkPlugin())
        return plugin->name();

    NX_ASSERT(false, "Unable to access the plugin SDK object");
    return QString();
}

QString pluginLibName(const nx::vms::server::resource::AnalyticsEngineResourcePtr& engineResource)
{
    const auto pluginResource =
        engineResource->plugin().dynamicCast<nx::vms::server::resource::AnalyticsPluginResource>();

    if (!NX_ASSERT(pluginResource))
        return QString();

    return pluginLibName(pluginResource);
}

QString filename(
    const QnVirtualCameraResourcePtr& device,
    const nx::vms::server::resource::AnalyticsEngineResourcePtr& engineResource,
    const QString& postfix)
{
    if (!NX_ASSERT(engineResource))
        return QString();

    if (!NX_ASSERT(device))
        return QString();

    const auto libName = pluginLibName(engineResource);
    if (!NX_ASSERT(!libName.isEmpty()))
        return QString();

    return libName + "_device_" + device->getId().toSimpleString() + postfix;
}

QString filename(
    const nx::vms::server::resource::AnalyticsEngineResourcePtr& engineResource,
    const QString& postfix)
{
    if (!NX_ASSERT(engineResource))
        return QString();

    const auto libName = pluginLibName(engineResource);
    if (!NX_ASSERT(!libName.isEmpty()))
        return QString();

    return libName + "_engine" + postfix;
}

QString buildFilename(const QString& libName, const QString& postfix)
{
    if (!NX_ASSERT(!libName.isEmpty()))
        return QString();

    if (!NX_ASSERT(!postfix.isEmpty()))
        return QString();

    return libName + postfix;
}

QString debugFilesDirectoryPath(const QString& path)
{
    if (QDir::isAbsolutePath(path))
        return QDir::cleanPath(path);

    const QString basePath(nx::kit::IniConfig::iniFilesDir());
    const QString fullPath = basePath + path;

    const QDir directory(fullPath);
    if (!directory.exists())
    {
        NX_WARNING(kLogTag, "Directory doesn't exist: %1", directory.absolutePath());
        return QString();
    }

    return directory.absolutePath();
}

} // namespace

nx::sdk::common::Ptr<nx::sdk::IStringMap> loadSettingsFromFile(
    const QString& fileDescription,
    const QString& filename)
{
    using nx::utils::log::Level;
    auto log = //< Can be used to return empty settings: return log(...)
        [&](Level level, const QString& message)
        {
            NX_UTILS_LOG(level, kLogTag) << lm("Metadata %1 settings: %2: [%3]")
                .args(fileDescription, message, filename);
            return nullptr;
        };

    if (filename.isEmpty())
        return log(Level::error, lit("File name is empty"));

    if (!QFileInfo::exists(filename))
        return log(Level::info, lit("File does not exist"));

    log(Level::info, lit("Loading from file"));

    QFile f(filename);
    if (!f.open(QFile::ReadOnly))
        return log(Level::error, lit("Unable to open file"));

    const QString& settingsJson = f.readAll();
    if (settingsJson.isEmpty())
        return log(Level::error, lit("Unable to read from file"));

    auto settings = sdk_support::toIStringMap(settingsJson);
    if (!settings)
        return log(Level::error, lit("Invalid JSON in file"));

    return settings;
}

nx::sdk::common::Ptr<nx::sdk::IStringMap> loadDeviceAgentSettingsFromFile(
    const QnVirtualCameraResourcePtr& device,
    const nx::vms::server::resource::AnalyticsEngineResourcePtr& engine)
{
    const auto settingsFilename = filename(device, engine, kSettingsFilenamePostfix);
    if (settingsFilename.isEmpty())
        return nullptr;

    if (!NX_ASSERT(pluginsIni().analyticsDeviceAgentSettingsPath[0]))
        return nullptr;

    const QDir dir(debugFilesDirectoryPath(pluginsIni().analyticsDeviceAgentSettingsPath));
    return loadSettingsFromFile(
        "DeviceAgent settings",
        dir.absoluteFilePath(settingsFilename));
}

nx::sdk::common::Ptr<nx::sdk::IStringMap> loadEngineSettingsFromFile(
    const nx::vms::server::resource::AnalyticsEngineResourcePtr& engine)
{
    const auto settingsFilename = filename(engine, kSettingsFilenamePostfix);
    if (settingsFilename.isEmpty())
        return nullptr;

    if (!NX_ASSERT(pluginsIni().analyticsEngineSettingsPath[0]))
        return nullptr;

    const QDir dir(debugFilesDirectoryPath(pluginsIni().analyticsEngineSettingsPath));
    return loadSettingsFromFile(
        "Engine settings",
        dir.absoluteFilePath(settingsFilename));
}

QString filename(
    const QnVirtualCameraResourcePtr& device,
    const nx::vms::server::resource::AnalyticsEngineResourcePtr& engine,
    const nx::vms::server::resource::AnalyticsPluginResourcePtr& plugin,
    const QString& postfix)
{
    if (device && engine)
        return filename(device, engine, postfix);
    else if (engine)
        return filename(engine, postfix);
    else if (plugin)
        return buildFilename(pluginLibName(plugin), postfix);

    NX_ASSERT(false, "Incorrect parameters");
    return QString();
}

QString filename(const nx::sdk::analytics::IPlugin* plugin, const QString& postfix)
{
    if (!NX_ASSERT(plugin))
        return QString();

    return buildFilename(pluginLibName(plugin), postfix);
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

    QDir dir(analytics::debug_helpers::debugFilesDirectoryPath(directoryPath));
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

} // namespace nx::vms::server::analytics::debug_helpers
