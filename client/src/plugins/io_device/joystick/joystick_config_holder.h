#pragma once

#include "joystick_config.h"

#include <boost/optional/optional.hpp>

namespace nx {
namespace client {
namespace plugins {
namespace io_device {
namespace joystick {
namespace config {


class ConfigHolder
{
public:
    std::vector<Configuration> configurations();
    bool addConfiguration(const Configuration& configuration);
    bool removeConfiguration(const QString& configurationId);
    bool updateConfiguration(const Configuration& configuration);

    boost::optional<Configuration> getActiveConfigurationForJoystick(const QString& joystickId) const;
    bool setActiveConfigurationForJoystick(const QString& joystickId, const QString& configurationId);

    std::vector<Rule> controlEventRules(
        const QString& configurationId,
        const QString& controlId) const;

    std::map<QString, QString> controlOverrides(
        const QString& configurationId,
        const QString& controlId) const;

    bool load(const QString& configName);

private:
    bool doesConfigurationExistUnsafe(const QString& configurationId) const;

private:
    Config m_config;
    mutable QnMutex m_mutex;
};

} // namespace config
} // namespace joystick
} // namespace io_device
} // namespace plugins
} // namespace client
} // namespace nx