#include "analytics_engine_resource.h"
#include "analytics_plugin_resource.h"

#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>

#include <nx/vms/api/analytics/engine_manifest.h>

#include <nx/fusion/model_functions.h>

#include <nx/analytics/types.h>
#include <nx/analytics/utils.h>

namespace nx::vms::common {

using namespace nx::vms::api::analytics;

const QString AnalyticsEngineResource::kSettingsValuesProperty{"settingsValues"};
const QString AnalyticsEngineResource::kEngineManifestProperty{"engineManifest"};

AnalyticsEngineResource::AnalyticsEngineResource(QnCommonModule* commonModule):
    base_type(commonModule)
{
}

EngineManifest AnalyticsEngineResource::manifest() const
{
    return QJson::deserialized(getProperty(kEngineManifestProperty).toUtf8(), EngineManifest());
}

void AnalyticsEngineResource::setManifest(const api::analytics::EngineManifest& manifest)
{
    setProperty(kEngineManifestProperty, QString::fromUtf8(QJson::serialized(manifest)));
}

QVariantMap AnalyticsEngineResource::settingsValues() const
{
    return QJsonDocument::fromJson(
        getProperty(kSettingsValuesProperty).toUtf8()).object().toVariantMap();
}

void AnalyticsEngineResource::setSettingsValues(const QVariantMap& values)
{
    setProperty(kSettingsValuesProperty,
        QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(values)).toJson()));
}

AnalyticsPluginResourcePtr AnalyticsEngineResource::plugin() const
{
    auto common = commonModule();
    if (!NX_ASSERT(common, "Can't access common module"))
        return AnalyticsPluginResourcePtr();

    return common->resourcePool()->getResourceById(getParentId())
        .dynamicCast<AnalyticsPluginResource>();
}

EventTypeDescriptorMap AnalyticsEngineResource::analyticsEventTypeDescriptors() const
{
    const auto engineManifest = manifest();
    return nx::analytics::fromManifestItemListToDescriptorMap<EventTypeDescriptor>(
        getId(),
        engineManifest.eventTypes);
}

ObjectTypeDescriptorMap AnalyticsEngineResource::analyticsObjectTypeDescriptors() const
{
    const auto engineManifest = manifest();
    return nx::analytics::fromManifestItemListToDescriptorMap<ObjectTypeDescriptor>(
        getId(),
        engineManifest.objectTypes);
}

} // namespace nx::vms::common
