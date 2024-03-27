// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonObject>

#include <core/resource/resource.h>
#include <nx/utils/value_cache.h>
#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/api/analytics/object_action.h>
#include <nx/vms/api/data/analytics_integration_model.h>

namespace nx::vms::common {

/**
 * VMS Resource corresponding to all instances (System-wide) of a particular Engine of an
 * Analytics Plugin.
 */
class NX_VMS_COMMON_API AnalyticsEngineResource: public QnResource
{
    Q_OBJECT

    using base_type = QnResource;

public:
    static const QString kSettingsValuesProperty;
    static const QString kSettingsModelProperty;
    static const QString kEngineManifestProperty;

    AnalyticsEngineResource();

    vms::api::analytics::EngineManifest manifest() const;
    void setManifest(const vms::api::analytics::EngineManifest& manifest);

    virtual QString idForToStringFromPtr() const override;

    AnalyticsPluginResourcePtr plugin() const;

    virtual std::set<QString> eventTypeIds() const;
    virtual std::set<QString> objectTypeIds() const;

    QList<nx::vms::api::analytics::ObjectAction> supportedObjectActions() const;

    /**
     * Device-dependent Engines are always running on a Device and cannot be disabled by the user.
     */
    virtual bool isDeviceDependent() const;

    virtual nx::vms::api::analytics::IntegrationType integrationType() const;

    bool isEnabledForDevice(const QnVirtualCameraResourcePtr& device) const;

signals:
    void manifestChanged(const nx::vms::common::AnalyticsEngineResourcePtr& engine);

private:
    vms::api::analytics::EngineManifest fetchManifest() const;

private:
    nx::utils::CachedValue<vms::api::analytics::EngineManifest> m_cachedManifest;
};

} // namespace nx::vms::common
