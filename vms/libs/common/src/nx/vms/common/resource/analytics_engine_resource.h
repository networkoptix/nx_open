#pragma once

#include <QtCore/QVariantMap>

#include <core/resource/resource.h>
#include <nx/vms/api/analytics/engine_manifest.h>

namespace nx::vms::common {

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
};

} // namespace nx::vms::common

Q_DECLARE_METATYPE(nx::vms::common::AnalyticsEngineResourcePtr)
Q_DECLARE_METATYPE(nx::vms::common::AnalyticsEngineResourceList)
