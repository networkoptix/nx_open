#include "analytics_engine_resource.h"
#include "analytics_plugin_resource.h"

#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>

namespace nx::vms::common {

QString AnalyticsEngineResource::kSettingsValuesProperty("settingsValues");

AnalyticsEngineResource::AnalyticsEngineResource(QnCommonModule* commonModule):
    base_type(commonModule)
{
}

AnalyticsEngineResource::~AnalyticsEngineResource() = default;

api::analytics::EngineManifest AnalyticsEngineResource::manifest() const
{
    return api::analytics::EngineManifest();
    // TODO: #dmishin implement
}

void AnalyticsEngineResource::setManifest(const api::analytics::EngineManifest& manifest)
{
    // TODO: #dmishin implement
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
