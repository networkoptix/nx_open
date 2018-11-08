#include "analytics_engine_resource.h"

#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>

namespace nx::vms::common {

QString AnalyticsEngineResource::kSettingsValuesProperty("settingsValues");

AnalyticsEngineResource::AnalyticsEngineResource(QnCommonModule* commonModule):
    base_type(commonModule)
{
}

AnalyticsEngineResource::~AnalyticsEngineResource() = default;

QVariantMap AnalyticsEngineResource::settingsValues() const
{
    return QJsonDocument::fromJson(
        getProperty(kSettingsValuesProperty).toUtf8()).object().toVariantMap();
}

void AnalyticsEngineResource::setSettingsValues(const QVariantMap& values)
{
    setProperty(kSettingsValuesProperty,
        QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(values)).toJson()));
}

} // namespace nx::vms::common
