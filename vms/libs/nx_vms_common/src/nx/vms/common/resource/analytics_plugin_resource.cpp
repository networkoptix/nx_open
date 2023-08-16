// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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

AnalyticsPluginResource::AnalyticsPluginResource():
    base_type()
{
}

PluginManifest AnalyticsPluginResource::manifest() const
{
    return QJson::deserialized(getProperty(
        nx::vms::api::analytics::kPluginManifestProperty).toUtf8(),
        PluginManifest());
}

void AnalyticsPluginResource::setManifest(const PluginManifest& manifest)
{
    setProperty(
        nx::vms::api::analytics::kPluginManifestProperty,
        QString::fromUtf8(QJson::serialized(manifest)));
}

AnalyticsEngineResourceList AnalyticsPluginResource::engines() const
{
    auto resourcePool = this->resourcePool();
    if (!NX_ASSERT(resourcePool))
        return AnalyticsEngineResourceList();

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

nx::vms::api::analytics::IntegrationType AnalyticsPluginResource::integrationType() const
{
    return nx::reflect::fromString(
        getProperty(kIntegrationTypeProperty).toStdString(),
        nx::vms::api::analytics::IntegrationType::sdk);
}

QString AnalyticsPluginResource::idForToStringFromPtr() const
{
    return nx::format("[%1 %2]").args(getName(), getId());
}

QnUuid AnalyticsPluginResource::uuidFromManifest(const QString& id)
{
    return QnUuid::fromArbitraryData(id);
}

} // namespace nx::vms::common
