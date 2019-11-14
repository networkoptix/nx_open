#pragma once

#include <QtCore/QJsonObject>

#include <core/resource/resource.h>
#include <nx/utils/value_cache.h>

#include <nx/analytics/types.h>
#include <nx/vms/api/analytics/engine_manifest.h>

namespace nx::vms::common {

/**
 * VMS Resource corresponding to all instances (System-wide) of a particular Engine of an
 * Analytics Plugin.
 */
class AnalyticsEngineResource: public QnResource
{
    Q_OBJECT

    using base_type = QnResource;

public:
    static const QString kSettingsValuesProperty;
    static const QString kEngineManifestProperty;

    AnalyticsEngineResource(QnCommonModule* commonModule = nullptr);

    api::analytics::EngineManifest manifest() const;
    void setManifest(const api::analytics::EngineManifest& manifest);

    virtual QJsonObject settingsValues() const;
    virtual void setSettingsValues(const QJsonObject& values);
    virtual QString idForToStringFromPtr() const override;

    AnalyticsPluginResourcePtr plugin() const;

    nx::analytics::EventTypeDescriptorMap analyticsEventTypeDescriptors() const;
    nx::analytics::ObjectTypeDescriptorMap analyticsObjectTypeDescriptors() const;

    QList<nx::vms::api::analytics::EngineManifest::ObjectAction> supportedObjectActions() const;

    /**
     * Device-dependent Engines are always running on a Device and cannot be disabled by the user.
     */
    bool isDeviceDependent() const;

    bool isEnabledForDevice(const QnVirtualCameraResourcePtr& device) const;

signals:
    void manifestChanged(const nx::vms::common::AnalyticsEngineResourcePtr& engine);

private:
    api::analytics::EngineManifest fetchManifest() const;

private:
    nx::utils::CachedValue<api::analytics::EngineManifest> m_cachedManifest;
};

} // namespace nx::vms::common

Q_DECLARE_METATYPE(nx::vms::common::AnalyticsEngineResourcePtr)
Q_DECLARE_METATYPE(nx::vms::common::AnalyticsEngineResourceList)
