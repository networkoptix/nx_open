#pragma once

#include <vector>

#include <QtCore/QJsonObject>

#include <core/resource/resource.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/api/analytics/plugin_manifest.h>

namespace nx::vms::common {

class AnalyticsPluginResource: public QnResource
{
    using base_type = QnResource;

public:
    static QString kPluginManifestProperty;
    static QString kDeviceAgentSettingsModelProperty;
    static QString kEngineSettingsModelProperty;

    AnalyticsPluginResource(QnCommonModule* commonModule = nullptr);
    virtual ~AnalyticsPluginResource() override = default;

    api::analytics::PluginManifest manifest() const;
    void setManifest(const api::analytics::PluginManifest& manifest);

    QJsonObject deviceAgentSettingsModel() const;
    void setDeviceAgentSettingsModel(QJsonObject model);

    QJsonObject engineSettingsModel() const;
    void setEngineSettingsModel(QJsonObject model);

    AnalyticsEngineResourceList engines() const;
    bool hasDefaultEngine() const;
};

} // namespace nx::vms::common

Q_DECLARE_METATYPE(nx::vms::common::AnalyticsPluginResourcePtr)
Q_DECLARE_METATYPE(nx::vms::common::AnalyticsPluginResourceList)
