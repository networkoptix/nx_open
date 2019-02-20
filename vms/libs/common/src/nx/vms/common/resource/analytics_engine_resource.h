#pragma once

#include <QtCore/QVariantMap>

#include <core/resource/resource.h>
#include <utils/common/value_cache.h>

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

    virtual QVariantMap settingsValues() const;
    virtual void setSettingsValues(const QVariantMap& values);
    virtual QString idForToStringFromPtr() const override;

    AnalyticsPluginResourcePtr plugin() const;

    nx::analytics::EventTypeDescriptorMap analyticsEventTypeDescriptors() const;
    nx::analytics::ObjectTypeDescriptorMap analyticsObjectTypeDescriptors() const;

    QList<nx::vms::api::analytics::EngineManifest::ObjectAction> supportedObjectActions() const;

    bool isDeviceDependent() const;

    bool isEnabledForDevice(const QnVirtualCameraResourcePtr& device) const;

signals:
    void manifestChanged(const nx::vms::common::AnalyticsEngineResourcePtr& engine);

private:
    api::analytics::EngineManifest fetchManifest() const;

private:
    CachedValue<api::analytics::EngineManifest> m_cachedManifest;
    mutable QnMutex m_cacheMutex;
};

} // namespace nx::vms::common

Q_DECLARE_METATYPE(nx::vms::common::AnalyticsEngineResourcePtr)
Q_DECLARE_METATYPE(nx::vms::common::AnalyticsEngineResourceList)
