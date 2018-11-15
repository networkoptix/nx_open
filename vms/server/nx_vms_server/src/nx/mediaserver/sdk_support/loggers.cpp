#include "loggers.h"

#include <plugins/plugins_ini.h>

#include <core/resource/camera_resource.h>
#include <nx/mediaserver/resource/analytics_engine_resource.h>
#include <nx/mediaserver/sdk_support/utils.h>
#include <nx/mediaserver/sdk_support/placeholder_binder.h>

namespace nx::mediaserver::sdk_support {

namespace {

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

QString buildManifestFileName(
    const QnVirtualCameraResourcePtr& device,
    const nx::vms::common::AnalyticsEngineResourcePtr& engine,
    const nx::vms::common::AnalyticsPluginResourcePtr& plugin)
{
    auto normalize =
        [](const QString& s)
        {
            QString result = s.toLower().replace(' ', '_');
            return result;
        };

    QStringList resultParts;
    if (device)
        resultParts << "device_" + normalize(device->getUserDefinedName());

    auto pluginResource = plugin;
    if (engine)
    {
        resultParts << "engine_" + engine->getId().toString();
        pluginResource = engine->plugin();
    }

    if (pluginResource)
        resultParts << "plugin_" + normalize(pluginResource->getName());

    auto result = resultParts.join('_');
    if (result.isEmpty())
        result = "unknown_manifest";

    return result;
}

} // namespace

//-----------------------------------------------------------------------------------------------

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
        sdk_support::saveManifestToFile(
            m_logTag,
            manifestStr,
            buildManifestFileName(m_device, m_engineResource, m_pluginResource));
    }

    if (error != nx::sdk::Error::noError)
    {
        NX_WARNING(m_logTag, buildLogString(
            customError.isEmpty() ? nx::sdk::toString(error) : customError));
    }
}

QString ManifestLogger::buildLogString(const QString& error) const
{
    sdk_support::PlaceholderBinder binder(m_messageTemplate);
    binder.bind({
        {"device", buildDeviceStr(m_device)},
        {"engine", buildEngineStr(m_engineResource)},
        {"plugin", buildPluginStr(m_pluginResource)},
        {"error", error},
    });

    return binder.str();
}

} // namespace nx::mediaserver::sdk_support
