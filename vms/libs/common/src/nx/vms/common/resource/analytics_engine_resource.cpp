#include "analytics_engine_resource.h"
#include "analytics_plugin_resource.h"

#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>

#include <common/common_module.h>
#include <core/resource/camera_resource.h>
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
    base_type(commonModule),
    m_cachedManifest([this]() { return fetchManifest(); }, &m_cacheMutex)
{
    connect(
        this,
        &QnResource::propertyChanged,
        this,
        [&](auto& resource, auto& key)
        {
            if (key == kEngineManifestProperty)
            {
                m_cachedManifest.reset();
                emit manifestChanged(toSharedPointer(this));
            }
        });
}

EngineManifest AnalyticsEngineResource::manifest() const
{
    return m_cachedManifest.get();
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

QString AnalyticsEngineResource::idForToStringFromPtr() const
{
    return lm("[%1 %2]").args(getName(), getId());
}

AnalyticsPluginResourcePtr AnalyticsEngineResource::plugin() const
{
    auto common = commonModule();
    if (!NX_ASSERT(common, "Can't access common module"))
        return AnalyticsPluginResourcePtr();

    return common->resourcePool()->getResourceById(getParentId())
        .dynamicCast<AnalyticsPluginResource>();
}

analytics::EventTypeDescriptorMap AnalyticsEngineResource::analyticsEventTypeDescriptors() const
{
    const auto engineManifest = manifest();
    return analytics::fromManifestItemListToDescriptorMap<EventTypeDescriptor>(
        getId(),
        engineManifest.eventTypes);
}

analytics::ObjectTypeDescriptorMap AnalyticsEngineResource::analyticsObjectTypeDescriptors() const
{
    const auto engineManifest = manifest();
    return analytics::fromManifestItemListToDescriptorMap<ObjectTypeDescriptor>(
        getId(),
        engineManifest.objectTypes);
}

QList<nx::vms::api::analytics::EngineManifest::ObjectAction>
    AnalyticsEngineResource::supportedObjectActions() const
{
    return manifest().objectActions;
}

bool AnalyticsEngineResource::isDeviceDependent() const
{
    return manifest().capabilities.testFlag(EngineManifest::Capability::deviceDependent);
}

bool AnalyticsEngineResource::isEnabledForDevice(const QnVirtualCameraResourcePtr& device) const
{
    return device->enabledAnalyticsEngines().contains(getId());
}

api::analytics::EngineManifest AnalyticsEngineResource::fetchManifest() const
{
    return QJson::deserialized<EngineManifest>(getProperty(kEngineManifestProperty).toUtf8());
}

} // namespace nx::vms::common
