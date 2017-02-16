#pragma once

#include "joystick_mapping.h"

#include <boost/optional/optional.hpp>

namespace nx {
namespace joystick {
namespace mapping {


class ConfigHolder
{
public:
    std::vector<JoystickConfiguration> getConfigurations();
    bool addConfiguration(const JoystickConfiguration& configuration);
    bool removeConfiguration(const QString& configurationId);
    bool updateConfiguration(const JoystickConfiguration& configuration);

    boost::optional<JoystickConfiguration> getActiveConfigurationForJoystick(const QString& joystickId) const;
    bool setActiveConfigurationForJoystick(const QString& joystickId, const QString& configurationId);

    std::vector<Rule> getControlEventRules(
        const QString& configurationId,
        const QString& controlId) const;

    std::map<QString, QString> getControlOverrides(
        const QString& configurationId,
        const QString& controlId) const;

    bool load();

private:
    bool checkIfConfigurationExistsUnsafe(const QString& configurationId) const;

private:
    Config m_config;
    mutable QnMutex m_mutex;
};

} // namespace mapping
} // namespace joystick
} // namespace nx