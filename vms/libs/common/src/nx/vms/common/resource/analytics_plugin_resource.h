#pragma once

#include <QtCore/QJsonObject>

#include <core/resource/resource.h>

namespace nx::vms::common {

class AnalyticsPluginResource: public QnResource
{
    using base_type = QnResource;

public:
    static QString kDeviceAgentSettingsModelProperty;
    static QString kEngineSettingsModelProperty;

    AnalyticsPluginResource(QnCommonModule* commonModule = nullptr);
    virtual ~AnalyticsPluginResource() override;

    QJsonObject deviceAgentSettingsModel() const;
    QJsonObject engineSettingsModel() const;
};

} // namespace nx::vms::common

Q_DECLARE_METATYPE(nx::vms::common::AnalyticsPluginResourcePtr)
Q_DECLARE_METATYPE(nx::vms::common::AnalyticsPluginResourceList)
