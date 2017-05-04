#pragma once

#include <set>

#include <nx/client/desktop/ui/actions/actions.h>
#include <plugins/io_device/joystick/joystick_common.h>
#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace client {
namespace plugins {
namespace io_device {
namespace joystick {
namespace config {

struct Rule
{
    Rule() {};
    Rule(
        QString id,
        EventType evtType,
        desktop::ui::action::IDType actType,
        std::map<QString, QString> actParameters):

        ruleId(id),
        eventType(evtType),
        actionType(actType),
        actionParameters(actParameters)
    {};

    QString ruleId;
    EventType eventType;
    desktop::ui::action::IDType actionType;
    std::map<QString, QString> actionParameters;
};

#define Rule_Fields (ruleId)(eventType)(actionType)(actionParameters)
QN_FUSION_DECLARE_FUNCTIONS(
    nx::client::plugins::io_device::joystick::config::Rule, (json));

struct Configuration
{
    typedef QString ControlIdType;
    typedef QString OverrideNameType;

    QString configurationId;
    QString configurationName;
    std::map<ControlIdType, std::map<OverrideNameType, QString>> controlOverrides;
    std::map<ControlIdType, std::vector<Rule>> eventMapping;
};

#define Configuration_Fields (configurationId)(configurationName)(controlOverrides)(eventMapping)
QN_FUSION_DECLARE_FUNCTIONS(
    nx::client::plugins::io_device::joystick::config::Configuration, (json));


struct Config
{
    typedef QString JoystickIdType;
    typedef QString ConfigurationIdType;

    std::map<JoystickIdType, ConfigurationIdType> enabledConfigurations;
    std::map<ConfigurationIdType, Configuration> configurations;
};

#define Config_Fields (enabledConfigurations)(configurations)
QN_FUSION_DECLARE_FUNCTIONS(
    nx::client::plugins::io_device::joystick::config::Config, (json));

const QString kPresetIndexParameterName = lit("presetIndex");

} // namespace config
} // namespace joystick
} // namespace io_device
} // namespace plugins
} // namespace client
} // namespace nx