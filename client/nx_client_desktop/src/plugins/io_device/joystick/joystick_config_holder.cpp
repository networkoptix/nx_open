#include "joystick_config_holder.h"

#include <QtCore/QFile>

#include <nx/fusion/model_functions.h>

namespace nx {
namespace client {
namespace plugins {
namespace io_device {
namespace joystick {
namespace config {

std::vector<Configuration> ConfigHolder::configurations()
{
    QnMutexLocker lock(&m_mutex);
    std::vector<Configuration> configurations;
    for (const auto& configPair: m_config.configurations)
        configurations.push_back(configPair.second);

    return configurations;
}

bool ConfigHolder::addConfiguration(const Configuration& configuration)
{
    QnMutexLocker lock(&m_mutex);

    if (doesConfigurationExistUnsafe(configuration.configurationId))
        return false;

    m_config.configurations[configuration.configurationId] = configuration;
    return true;
}

bool ConfigHolder::removeConfiguration(const QString& configurationId)
{
    QnMutexLocker lock(&m_mutex);
    return m_config.configurations.erase(configurationId);
}

bool ConfigHolder::updateConfiguration(const Configuration& configuration)
{
    QnMutexLocker lock(&m_mutex);
    auto itr = m_config.configurations.find(configuration.configurationId);
    if ( itr == m_config.configurations.end())
        return false;

    m_config.configurations[configuration.configurationId] = configuration;
    return true;
}

boost::optional<Configuration> ConfigHolder::getActiveConfigurationForJoystick(
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

bool ConfigHolder::setActiveConfigurationForJoystick(
    const QString& joystickId,
    const QString& configurationId)
{
    QnMutexLocker lock(&m_mutex);
    auto itr = m_config.configurations.find(configurationId);
    if (itr == m_config.configurations.end())
        return false;

    m_config.enabledConfigurations[joystickId] = configurationId;
    return true;
}

std::vector<Rule> ConfigHolder::controlEventRules(
    const QString& configurationId,
    const QString& controlId) const
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

std::map<QString, QString> ConfigHolder::controlOverrides(
    const QString& configurationId,
    const QString& controlId) const
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

bool ConfigHolder::doesConfigurationExistUnsafe(const QString& configurationId) const
{
    return m_config.configurations.find(configurationId) != m_config.configurations.end();
}

bool ConfigHolder::load(const QString& configPath)
{
    QFile configFile(configPath);
    if (!configFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QByteArray data = configFile.readAll();

    QnMutexLocker lock(&m_mutex);
    m_config = Config();
    return QJson::deserialize(data, &m_config);
}

} // namespace config
} // namespace joystick
} // namespace io_device
} // namespace plugins
} // namespace client
} // namespace nx