#include "settings.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace plugins {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    Setting,
    (json),
    Setting_Fields,
    (brief, true));

void SettingsHolder::fillSettingsFromData()
{
    for (const auto& setting: m_data)
        m_settings.emplace_back(setting.name.constData(), setting.value.constData());
}

SettingsHolder::SettingsHolder(const QSettings* settings): m_isValid(true)
{
    for (const auto& key: settings->allKeys())
        m_data.append({key.toUtf8(), settings->value(key).toString().toUtf8()});
    fillSettingsFromData();
}

SettingsHolder::SettingsHolder(const QString& keyValueJson)
{
    m_data = QJson::deserialized<QList<Setting>>(
        keyValueJson.toUtf8(), /*defaultValue*/ {}, &m_isValid);
    fillSettingsFromData();
}

SettingsHolder::SettingsHolder(const QMap<QString, QString>& map)
{
    for (auto it = map.constBegin(); it != map.constEnd(); ++it)
        m_data.append({it.key().toUtf8(), it.value().toUtf8()});
    fillSettingsFromData();
}

} // namespace plugins
} // namespace nx
