#pragma once

#include <QtCore/QVariantMap>

#include <core/resource/resource.h>
#include <nx/analytics/types.h>
#include <nx/vms/api/analytics/engine_manifest.h>

namespace nx::vms::common {

/**
 * VMS Resource corresponding to all instances (System-wide) of a particular Engine of an
 * Analytics Plugin.
 */
class AnalyticsEngineResource: public QnResource
{
    using base_type = QnResource;

public:
    static const QString kSettingsValuesProperty;
    static const QString kEngineManifestProperty;

    AnalyticsEngineResource(QnCommonModule* commonModule = nullptr);

    api::analytics::EngineManifest manifest() const;
    void setManifest(const api::analytics::EngineManifest& manifest);

    virtual QVariantMap settingsValues() const;
    virtual void setSettingsValues(const QVariantMap& values);

    AnalyticsPluginResourcePtr plugin() const;

    nx::analytics::EventTypeDescriptorMap analyticsEventTypeDescriptors() const;
    nx::analytics::ObjectTypeDescriptorMap analyticsObjectTypeDescriptors() const;

    bool isDeviceDependent() const;

    bool isEnabledForDevice(const QnVirtualCameraResourcePtr& device) const;
};

} // namespace nx::vms::common

Q_DECLARE_METATYPE(nx::vms::common::AnalyticsEngineResourcePtr)
Q_DECLARE_METATYPE(nx::vms::common::AnalyticsEngineResourceList)
