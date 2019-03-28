#include "loggers.h"

#include <plugins/plugins_ini.h>

#include <core/resource/camera_resource.h>
#include <nx/vms/server/resource/analytics_engine_resource.h>
#include <nx/vms/server/analytics/debug_helpers.h>
#include <nx/vms/server/sdk_support/utils.h>
#include <nx/utils/placeholder_binder.h>
#include <nx/sdk/helpers/to_string.h>

namespace nx::vms::server::sdk_support {

namespace {

static const QString kManifestFilenamePostfix("_manifest.json");
static const QString kPluginPlaceholder("{:plugin}");
static const QString kDevicePlaceholder("{:device}");
static const QString kEnginePlaceholder("{:engine}");
static const QString kErrorPlaceholder("{:error}");

QString buildDeviceStr(const QnVirtualCameraResourcePtr& device)
{
    if (!device)
        return QString();

    return lm("%1 (%2)").args(device->getUserDefinedName(), device->getId());
}

QString buildEngineStr(const resource::AnalyticsEngineResourcePtr& engine)
{
    if (!engine)
        return QString();

    const auto plugin = engine->plugin();
    return lm("%1 (%2)").args(
        plugin ? plugin->getName() : QString("Unknown plugin"),
        engine->getId());
}

QString buildPluginStr(const resource::AnalyticsPluginResourcePtr& plugin)
{
    if (!plugin)
        return QString();

    return lm("%1 (%2)").args(plugin->getName(), plugin->getId());
}

} // namespace

ManifestLogger::ManifestLogger(
    nx::utils::log::Tag logTag,
    QString messageTemplate,
    resource::AnalyticsPluginResourcePtr pluginResource)
    :
    m_logTag(std::move(logTag)),
    m_messageTemplate(std::move(messageTemplate)),
    m_pluginResource(std::move(pluginResource))
{
}

ManifestLogger::ManifestLogger(
    nx::utils::log::Tag logTag,
    QString messageTemplate,
    resource::AnalyticsEngineResourcePtr engineResource)
    :
    m_logTag(std::move(logTag)),
    m_messageTemplate(std::move(messageTemplate)),
    m_pluginResource(engineResource->plugin().dynamicCast<resource::AnalyticsPluginResource>()),
    m_engineResource(std::move(engineResource))
{
}

ManifestLogger::ManifestLogger(
    nx::utils::log::Tag logTag,
    QString messageTemplate,
    QnVirtualCameraResourcePtr device,
    resource::AnalyticsEngineResourcePtr engineResource)
    :
    m_logTag(std::move(logTag)),
    m_messageTemplate(std::move(messageTemplate)),
    m_device(std::move(device)),
    m_pluginResource(engineResource->plugin().dynamicCast<resource::AnalyticsPluginResource>()),
    m_engineResource(std::move(engineResource))
{
}

void ManifestLogger::log(
    const QString& manifestStr,
    sdk::Error error,
    const QString& customError)
{
    if (pluginsIni().analyticsManifestOutputPath[0])
    {
        analytics::debug_helpers::dumpStringToFile(
            m_logTag,
            manifestStr,
            QString(pluginsIni().analyticsManifestOutputPath),
            analytics::debug_helpers::nameOfFileToDumpOrLoadData(
                m_device,
                m_engineResource,
                m_pluginResource,
                kManifestFilenamePostfix));
    }

    if (error != nx::sdk::Error::noError)
    {
        NX_WARNING(m_logTag, buildLogString(
            customError.isEmpty() ? nx::sdk::toString(error) : customError));
    }
}

QString ManifestLogger::buildLogString(const QString& error) const
{
    nx::utils::PlaceholderBinder binder(m_messageTemplate);
    binder.bind({
        {"device", buildDeviceStr(m_device)},
        {"engine", buildEngineStr(m_engineResource)},
        {"plugin", buildPluginStr(m_pluginResource)},
        {"error", error},
    });

    return binder.str();
}

//--------------------------------------------------------------------------------------------------

StartupPluginManifestLogger::StartupPluginManifestLogger(
    nx::utils::log::Tag logTag,
    const nx::sdk::analytics::IPlugin* plugin)
    :
    m_logTag(std::move(logTag)),
    m_plugin(plugin)
{
    NX_ASSERT(plugin);
}

void StartupPluginManifestLogger::log(
    const QString& manifestStr,
    nx::sdk::Error error,
    const QString& customError)
{
    if (pluginsIni().analyticsManifestOutputPath[0])
    {
        analytics::debug_helpers::dumpStringToFile(
            m_logTag,
            manifestStr,
            QString(pluginsIni().analyticsManifestOutputPath),
            analytics::debug_helpers::nameOfFileToDumpOrLoadData(
                m_plugin,
                kManifestFilenamePostfix));
    }

    if (error != nx::sdk::Error::noError)
    {
        NX_WARNING(m_logTag, buildLogString(
            customError.isEmpty() ? nx::sdk::toString(error) : customError));
    }
}

QString StartupPluginManifestLogger::buildLogString(const QString& error) const
{
    return lm("Error while plugin manifest from Plugin %1: %2").args(m_plugin->name(), error);
}

} // namespace nx::vms::server::sdk_support
