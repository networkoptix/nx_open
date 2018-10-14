#include "analytics_plugin_resource.h"

#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>

#include <nx/vms/common/resource/analytics_engine_resource.h>

namespace nx::vms::common {

QString AnalyticsPluginResource::kDeviceAgentSettingsModelProperty("deviceAgentSettingsModel");
QString AnalyticsPluginResource::kEngineSettingsModelProperty("engineSettingsModel");

AnalyticsPluginResource::AnalyticsPluginResource(QnCommonModule* commonModule):
    base_type(commonModule)
{
}

api::analytics::PluginManifest AnalyticsPluginResource::manifest() const
{
    // TODO: #dmishin implement.
    return api::analytics::PluginManifest();
}

void AnalyticsPluginResource::setManifest(const api::analytics::PluginManifest& manifest)
{
    // TODO: #dmishin implement.
}

QJsonObject AnalyticsPluginResource::deviceAgentSettingsModel() const
{
    return QJsonDocument::fromJson(
        getProperty(kDeviceAgentSettingsModelProperty).toUtf8()).object();
}

void AnalyticsPluginResource::setDeviceAgentSettingsModel(QJsonObject model)
{
    setProperty(kDeviceAgentSettingsModelProperty, model);
}

QJsonObject AnalyticsPluginResource::engineSettingsModel() const
{
    return QJsonDocument::fromJson(getProperty(kEngineSettingsModelProperty).toUtf8()).object();
}

void AnalyticsPluginResource::setEngineSettingsModel(QJsonObject model)
{
    setProperty(kEngineSettingsModelProperty, model);
}

AnalyticsEngineResourceList AnalyticsPluginResource::engines() const
{
    auto common = commonModule();
    if (!common)
    {
        NX_ASSERT(false, "Resource has no common module.");
        return AnalyticsEngineResourceList();
    }

    auto resourcePool = common->resourcePool();
    if (!resourcePool)
    {
        NX_ASSERT(false, "Can't access resource pool.");
        return AnalyticsEngineResourceList();
    }

    return resourcePool->getResources<AnalyticsEngineResource>(
        [id = getId()](const AnalyticsEngineResourcePtr& resource)
        {
            return resource->getId() == id;
        });
}

bool AnalyticsPluginResource::hasDefaultEngine() const
{
    // TODO: #dmishin take this value from manifest
    return true;
}

} // namespace nx::vms::common
