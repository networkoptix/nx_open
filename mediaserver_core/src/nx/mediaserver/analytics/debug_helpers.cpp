#include "debug_helpers.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>

#include <nx/utils/log/log.h>
#include <nx/utils/log/log_level.h>
#include <core/resource/camera_resource.h>

#include <nx/mediaserver/sdk_support/utils.h>

#include <plugins/plugins_ini.h>

namespace nx::mediaserver::analytics::debug_helpers {

namespace {

static nx::utils::log::Tag kLogTag(QString("AnalyticsDebugHelpers"));

} // namespace

sdk_support::UniquePtr<nx::sdk::Settings> loadSettingsFromFile(
    const QString& fileDescription,
    const QString& filename)
{
    using nx::utils::log::Level;
    auto log = //< Can be used to return empty settings: return log(...)
        [&](Level level, const QString& message)
        {
            NX_UTILS_LOG(level, kLogTag) << lm("Metadata %1 settings: %2: [%3]")
                .args(fileDescription, message, filename);
            return sdk_support::UniquePtr<nx::sdk::Settings>();
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

    auto settings = sdk_support::toSdkSettings(settingsJson);
    if (!settings)
        return log(Level::error, lit("Invalid JSON in file"));

    return settings;
}

QString settingsFilename(
    const char* const path,
    const QString& pluginLibName,
    const QString& extraSuffix)
{
    if (path[0] == 0)
    {
        NX_ASSERT(false, "Path to config directory is empty");
        return QString();
    }

    // Normalize to use forward slashes, as required by QFile.
    QString dir = QDir::cleanPath(QString::fromUtf8(path));
    if (!dir.isEmpty() && dir.at(dir.size() - 1) != '/')
        dir.append('/');
    return dir + pluginLibName + extraSuffix + lit(".json");
}

void setDeviceAgentSettings(
    sdk_support::SharedPtr<nx::sdk::analytics::DeviceAgent> deviceAgent,
    const QnVirtualCameraResourcePtr& device)
{
    if (!deviceAgent)
    {
        NX_ASSERT(false, "device agent is nullptr");
        return;
    }

    if (!device)
    {
        NX_ASSERT(false, "Device is nullptr");
        return;
    }

    const auto settings = loadSettingsFromFile(lit("DeviceAgent"), settingsFilename(
        pluginsIni().analyticsDeviceAgentSettingsPath,
        deviceAgent->engine()->plugin()->name(),
        lit("_device_agent_for_") + device->getPhysicalId()));
    deviceAgent->setSettings(settings.get());
}

void setEngineSettings(sdk_support::SharedPtr<nx::sdk::analytics::Engine> engine)
{
    if (!engine)
    {
        NX_ASSERT(false, "Engine is nullptr");
        return;
    }

    const auto settings = loadSettingsFromFile(lit("Engine"), settingsFilename(
        pluginsIni().analyticsEngineSettingsPath, engine->plugin()->name()));
    engine->setSettings(settings.get());
}

} // namespace nx::mediaserver::analytics::debug_helpers
