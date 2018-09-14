#include "settings.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace plugins {

namespace detail {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    Setting,
    (json),
    Setting_Fields);

} using namespace detail;

void SettingsHolder::fillSettingsFromData()
{
    NX_ASSERT(m_isValid);
    for (const auto& setting: m_data)
        m_settings.emplace_back(setting.name.c_str(), setting.value.c_str());
}

SettingsHolder::SettingsHolder(const QSettings* settings): m_isValid(true)
{
    for (const auto& key: settings->allKeys())
        m_data.emplace_back(key.toStdString(), settings->value(key).toString().toStdString());
    fillSettingsFromData();
}

SettingsHolder::SettingsHolder(const QString& keyValueJson)
{
    m_data = QJson::deserialized<std::vector<Setting>>(
        keyValueJson.toUtf8(), /*defaultValue*/ {}, &m_isValid);
    if (m_isValid)
        fillSettingsFromData();
}

SettingsHolder::SettingsHolder(const QMap<QString, QString>& map): m_isValid(true)
{
    for (auto it = map.constBegin(); it != map.constEnd(); ++it)
        m_data.emplace_back(it.key().toStdString(), it.value().toStdString());
    fillSettingsFromData();
}

} // namespace plugins
} // namespace nx
