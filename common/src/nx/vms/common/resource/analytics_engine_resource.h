#pragma once

#include <QtCore/QVariantMap>

#include <core/resource/resource.h>
#include <nx/vms/api/analytics/engine_manifest.h>

namespace nx::vms::common {

class AnalyticsEngineResource: public QnResource
{
    using base_type = QnResource;

public:
    static QString kSettingsValuesProperty;

    AnalyticsEngineResource(QnCommonModule* commonModule = nullptr);
    virtual ~AnalyticsEngineResource() override;

    api::analytics::EngineManifest manifest() const;
    void setManifest(const api::analytics::EngineManifest& manifest);

    QVariantMap settingsValues() const;
    void setSettingsValues(const QVariantMap& values);

    AnalyticsPluginResourcePtr plugin() const;

    /**
     * @return true if engine doesn't need to be assigned by the user and is assigned to every
     * resource. Self assignable engine can decide on its own which resources are supported by it.
     * A good example of self assignable engine is any camera (Hanwha, Axis, Hikvision, etc) plugin
     * engine.
     */
    bool isSelfAssignable() const;
};

} // namespace nx::vms::common

Q_DECLARE_METATYPE(nx::vms::common::AnalyticsEngineResourcePtr)
Q_DECLARE_METATYPE(nx::vms::common::AnalyticsEngineResourceList)
