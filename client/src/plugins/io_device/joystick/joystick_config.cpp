#include "joystick_config.h"

#include <utils/serialization/json.h>

namespace nx {
namespace joystick {
namespace mapping {

std::vector<JoystickConfiguration> ConfigHolder::getConfigurations()
{
    QnMutexLocker lock(&m_mutex);
    std::vector<JoystickConfiguration> configurations;
    for (const auto& configPair: m_config.configurations)
        configurations.push_back(configPair.second);

    return configurations;
}

bool ConfigHolder::addConfiguration(const JoystickConfiguration& configuration)
{
    QnMutexLocker lock(&m_mutex);

    if (checkIfConfigurationExistsUnsafe(configuration.configurationId))
        return false;

    m_config.configurations[configuration.configurationId] = configuration;
    return true;
}

bool ConfigHolder::removeConfiguration(const QString& configurationId)
{
    QnMutexLocker lock(&m_mutex);
    return m_config.configurations.erase(configurationId);
}

bool ConfigHolder::updateConfiguration(const JoystickConfiguration& configuration)
{
    QnMutexLocker lock(&m_mutex);
    auto itr = m_config.configurations.find(configuration.configurationId);
    if ( itr == m_config.configurations.end())
        return false;

    m_config.configurations[configuration.configurationId] = configuration;
    return true;
}

boost::optional<JoystickConfiguration> ConfigHolder::getActiveConfigurationForJoystick(
    const QString& joystickId) const
{
    QnMutexLocker lock(&m_mutex);
    auto itr = m_config.enabledConfigurations.find(joystickId);
    if ( itr == m_config.enabledConfigurations.end())
        return boost::none;

    auto configItr = m_config.configurations.find(itr->second);
    if (configItr == m_config.configurations.end())
        return boost::none;

    return configItr->second;
}

bool ConfigHolder::setActiveConfigurationForJoystick(const QString& joystickId, const QString& configurationId)
{
    QnMutexLocker lock(&m_mutex);
    auto itr = m_config.configurations.find(configurationId);
    if (itr == m_config.configurations.end())
        return false;

    m_config.enabledConfigurations[joystickId] = configurationId;
    return true;
}

std::vector<Rule> ConfigHolder::getControlEventRules(const QString& configurationId, const QString& controlId) const
{
    QnMutexLocker lock(&m_mutex);
    auto itr =  m_config.configurations.find(configurationId);
    if (itr == m_config.configurations.end())
        return std::vector<Rule>();

    auto& eventRulesMap = itr->second.eventMapping;

    auto rulesItr = eventRulesMap.find(controlId);
    if (rulesItr == eventRulesMap.end())
        return std::vector<Rule>();

    return rulesItr->second;
}

std::map<QString, QString> ConfigHolder::getControlOverrides(const QString& configurationId, const QString& controlId) const
{
    QnMutexLocker lock(&m_mutex);
    auto itr =  m_config.configurations.find(configurationId);
    if (itr == m_config.configurations.end())
        return std::map<QString, QString>();

    auto& overridesMap = itr->second.controlOverrides;

    auto overridesItr = overridesMap.find(controlId);
    if (overridesItr == overridesMap.end())
        return std::map<QString, QString>();

    return overridesItr->second;
}

bool ConfigHolder::checkIfConfigurationExistsUnsafe(const QString& configurationId) const
{
    return m_config.configurations.find(configurationId) != m_config.configurations.end();
}

bool ConfigHolder::load(const QString& configPath)
{
    bool opened = false;
    QFile configFile(configPath);
    if (!configFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QByteArray data = configFile.readAll();

    QnMutexLocker lock(&m_mutex);
    m_config = nx::joystick::mapping::Config();
    QJson::deserialize(data, &m_config);
    return true;
}

} // namespace mapping
} // namespace joystick
} // namespace nx