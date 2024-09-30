// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_engine_resource.h"
#include "analytics_plugin_resource.h"

#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>

#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/analytics/utils.h>
#include <nx/reflect/json/deserializer.h>
#include <nx/reflect/json/serializer.h>
#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/api/analytics/object_action.h>

namespace nx::vms::common {

using namespace nx::vms::api::analytics;

const QString AnalyticsEngineResource::kSettingsValuesProperty{"settingsValues"};
const QString AnalyticsEngineResource::kSettingsModelProperty{"settingsModel"};
const QString AnalyticsEngineResource::kEngineManifestProperty{"engineManifest"};

AnalyticsEngineResource::AnalyticsEngineResource():
    base_type(),
    m_cachedManifest([this]() { return fetchManifest(); })
{
    connect(
        this,
        &QnResource::propertyChanged,
        this,
        [&](auto& /*resource*/, auto& key)
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

void AnalyticsEngineResource::setManifest(const EngineManifest& manifest)
{
    setProperty(kEngineManifestProperty, QString::fromUtf8(nx::reflect::json::serialize(manifest)));
}

QString AnalyticsEngineResource::idForToStringFromPtr() const
{
    return nx::format("[%1 %2]").args(getName(), getId());
}

AnalyticsPluginResourcePtr AnalyticsEngineResource::plugin() const
{
    auto context = systemContext();
    if (!NX_ASSERT(context, "Can't access resource context"))
        return AnalyticsPluginResourcePtr();

    return resourcePool()->getResourceById(getParentId())
        .dynamicCast<AnalyticsPluginResource>();
}

std::set<QString> AnalyticsEngineResource::eventTypeIds() const
{
    const auto engineManifest = manifest();
    return analytics::fromManifestItemListToIds(std::vector<const QList<AnalyticsEventType>*>{
        &engineManifest.eventTypes,
        &engineManifest.typeLibrary.eventTypes});
}

std::set<QString> AnalyticsEngineResource::objectTypeIds() const
{
    const auto engineManifest = manifest();
    return analytics::fromManifestItemListToIds(std::vector<const QList<ObjectType>*>{
        &engineManifest.objectTypes,
        &engineManifest.typeLibrary.objectTypes});
}

QList<nx::vms::api::analytics::ObjectAction>
    AnalyticsEngineResource::supportedObjectActions() const
{
    return manifest().objectActions;
}

bool AnalyticsEngineResource::isDeviceDependent() const
{
    return manifest().capabilities.testFlag(EngineCapability::deviceDependent);
}

bool AnalyticsEngineResource::isEnabledForDevice(const QnVirtualCameraResourcePtr& device) const
{
    return device->enabledAnalyticsEngines().contains(getId());
}

EngineManifest AnalyticsEngineResource::fetchManifest() const
{
    auto [deserializedManifest, result] =
        nx::reflect::json::deserialize<EngineManifest>(getProperty(kEngineManifestProperty).toUtf8().toStdString());

    if (!result.success)
    {
        NX_WARNING(this, "Failed to deserialize manifest: %1", result.toString());
    }

    return deserializedManifest;
}

nx::vms::api::analytics::IntegrationType AnalyticsEngineResource::integrationType() const
{
    const AnalyticsPluginResourcePtr parentPlugin = plugin();
    if (!NX_ASSERT(parentPlugin))
        return nx::vms::api::analytics::IntegrationType::sdk;

    return parentPlugin->integrationType();
}

} // namespace nx::vms::common
