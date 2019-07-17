#include "loggers.h"

#include <plugins/vms_server_plugins_ini.h>

#include <core/resource/camera_resource.h>
#include <nx/vms/server/resource/analytics_engine_resource.h>
#include <nx/vms/server/analytics/debug_helpers.h>
#include <nx/vms/server/sdk_support/utils.h>
#include <nx/vms/server/sdk_support/to_string.h>
#include <nx/utils/placeholder_binder.h>

namespace nx::vms::server::sdk_support {

using namespace nx::sdk;

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
    const sdk_support::Error& error)
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

    if (!error.isOk())
    {
        nx::utils::PlaceholderBinder binder(m_messageTemplate);
        binder.bind({
            {"device", buildDeviceStr(m_device)},
            {"engine", buildEngineStr(m_engineResource)},
            {"plugin", buildPluginStr(m_pluginResource)},
            {"error", toString(error)},
        });
        NX_WARNING(m_logTag, binder.str());
    }
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
    const sdk_support::Error& error)
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

    if (!error.isOk())
    {
        NX_WARNING(m_logTag, "Error obtaining manifest from Plugin %1: %2",
            m_plugin->name(),
            error);
    }
}

} // namespace nx::vms::server::sdk_support
