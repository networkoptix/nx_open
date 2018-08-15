#pragma once

#include <vector>

#include <QtCore/QByteArray>
#include <QtCore/QMap>
#include <QtCore/QSettings>
#include <QtCore/QString>

#include <nx/fusion/model_functions.h>
#include <plugins/plugin_api.h>

namespace nx {
namespace plugins {

/**
 * Owns the strings. QByteArray is needed to easily obtain char* of each value for SDK purposes.
 */
struct Setting
{
    QByteArray name;
    QByteArray value;
};

#define Setting_Fields (name)(value)

QN_FUSION_DECLARE_FUNCTIONS(Setting, (json))

// TODO: #mshevchenko: Consider creating an empty object, and then use methods to load.
class SettingsHolder
{
  public:
    SettingsHolder() : m_isValid(true) {}
    SettingsHolder(const QSettings* settings);
    SettingsHolder(const QString& keyValueJson);
    SettingsHolder(const QMap<QString, QString>& map);

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
    QList<Setting> m_data; //< Owns the strings.
    std::vector<nxpl::Setting> m_settings; //< Points to strings from data.
    bool m_isValid = false; //< Set by constructors.
};

} // namespace plugins
} // namespace nx