#include "analytics_plugin_resource.h"

#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>

#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/api/analytics/plugin_manifest.h>

#include <nx/fusion/model_functions.h>

namespace nx::vms::common {

using namespace nx::vms::api::analytics;

QString AnalyticsPluginResource::kPluginManifestProperty("pluginManifest");

AnalyticsPluginResource::AnalyticsPluginResource(QnCommonModule* commonModule):
    base_type(commonModule)
{
}

PluginManifest AnalyticsPluginResource::manifest() const
{
    return QJson::deserialized(getProperty(kPluginManifestProperty).toUtf8(), PluginManifest());
}

void AnalyticsPluginResource::setManifest(const PluginManifest& manifest)
{
    setProperty(kPluginManifestProperty, QString::fromUtf8(QJson::serialized(manifest)));
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
