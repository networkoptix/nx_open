#pragma once

#include <vector>
#include <string>

#include <QtCore/QMap>
#include <QtCore/QSettings>
#include <QtCore/QString>

#include <nx/fusion/model_functions.h>
#include <plugins/plugin_api.h>

namespace nx {
namespace plugins {

namespace detail {

/**
 * Owns the 8-bit strings, which is needed to easily obtain char* of each value for SDK purposes.
 */
struct Setting
{
    std::string name;
    std::string value;

    Setting(std::string name = {}, std::string value = {}):
        name(std::move(name)), value(std::move(value))
    {
    }
};
#define Setting_Fields (name)(value)
QN_FUSION_DECLARE_FUNCTIONS(Setting, (json));

} using namespace detail;

//-------------------------------------------------------------------------------------------------

/**
 * Value object which stores name-value pairs and allows to obtain their char* as a C-style array
 * of nxpl::Setting for SDK purposes.
 */
class SettingsHolder
{
public:
    SettingsHolder(): m_isValid(true) {}
    SettingsHolder(const QSettings* settings);
    SettingsHolder(const QString& keyValueJson);
    SettingsHolder(const QMap<QString, QString>& map);

    SettingsHolder(const SettingsHolder&) = delete;
    SettingsHolder& operator=(const SettingsHolder&) = delete;

    bool isValid() const { return m_isValid; }

    const nxpl::Setting* array() const
    {
        return m_settings.empty() ? nullptr : &m_settings.front();
    }

    int size() const { return (int) m_settings.size(); }

    bool isEmpty() const { return m_settings.empty(); }

private:
    void fillSettingsFromData();

private:
    std::vector<Setting> m_data; //< Owns the strings.
    std::vector<nxpl::Setting> m_settings; //< Points to strings from data.
    bool m_isValid = false; //< Set by constructors.
};

} // namespace plugins
} // namespace nx
