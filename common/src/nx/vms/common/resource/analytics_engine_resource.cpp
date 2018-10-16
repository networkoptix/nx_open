#include "analytics_engine_resource.h"
#include "analytics_plugin_resource.h"

#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>

#include <nx/vms/api/analytics/engine_manifest.h>

namespace nx::vms::common {

using namespace nx::vms::api::analytics;

QString AnalyticsEngineResource::kSettingsValuesProperty("settingsValues");
QString AnalyticsEngineResource::kEngineManifestProperty("engineManifest");

AnalyticsEngineResource::AnalyticsEngineResource(QnCommonModule* commonModule):
    base_type(commonModule)
{
}

AnalyticsEngineResource::~AnalyticsEngineResource() = default;

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
    if (!common)
    {
        NX_ASSERT(false, "Can't access common module");
        return nullptr;
    }

    return common->resourcePool()->getResourceById(getParentId())
        .dynamicCast<AnalyticsPluginResource>();
}

bool AnalyticsEngineResource::isSelfAssignable() const
{
    // TODO: #dmishin take this value from manifest.
    return true;
}

} // namespace nx::vms::common
