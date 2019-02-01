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
#include <nx/analytics/device_descriptor_manager.h>

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
    const auto engineManifest = manifest();
    return engineManifest.objectActions;
}

bool AnalyticsEngineResource::isDeviceDependent() const
{
    const auto engineManifest = manifest();
    return engineManifest.capabilities.testFlag(EngineManifest::Capability::deviceDependent);
}

bool AnalyticsEngineResource::isEnabledForDevice(const QnVirtualCameraResourcePtr& device) const
{
    const auto engineId = getId();
    const auto userEnabledEngines = device->userEnabledAnalyticsEngines();
    if (userEnabledEngines.contains(engineId))
        return true;

    if (!isDeviceDependent())
        return false;

    nx::analytics::DeviceDescriptorManager deviceDescriptorManager(commonModule());
    const auto compatibleEngines = deviceDescriptorManager.compatibleEngineIds(device);

    return compatibleEngines.find(engineId) != compatibleEngines.cend();
}

} // namespace nx::vms::common
