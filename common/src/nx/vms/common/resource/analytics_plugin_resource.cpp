#include "analytics_plugin_resource.h"

#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>

namespace nx::vms::common {

QString AnalyticsPluginResource::kDeviceAgentSettingsModelProperty("deviceAgentSettingsModel");
QString AnalyticsPluginResource::kEngineSettingsModelProperty("engineSettingsModel");

AnalyticsPluginResource::AnalyticsPluginResource(QnCommonModule* commonModule):
    base_type(commonModule)
{
}

AnalyticsPluginResource::~AnalyticsPluginResource() = default;

QJsonObject AnalyticsPluginResource::deviceAgentSettingsModel() const
{
    return QJsonDocument::fromJson(
        getProperty(kDeviceAgentSettingsModelProperty).toUtf8()).object();
}

QJsonObject AnalyticsPluginResource::engineSettingsModel() const
{
    return QJsonDocument::fromJson(getProperty(kEngineSettingsModelProperty).toUtf8()).object();
}

} // namespace nx::vms::common
