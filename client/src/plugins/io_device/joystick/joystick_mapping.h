#pragma once

#include <set>

#include <ui/actions/action.h>
#include <plugins/io_device/joystick/joystick_common.h>
#include <utils/common/model_functions_fwd.h>

namespace nx {
namespace joystick {
namespace mapping {

struct Rule 
{
    Rule() {};
    Rule(
        QString id,
        nx::joystick::EventType evtType,
        QnActions::IDType actType,
        std::map<QString, QString> actParameters):
        
        ruleId(id),
        eventType(evtType),
        actionType(actType),
        actionParameters(actParameters)
    {};

    QString ruleId;
    nx::joystick::EventType eventType;
    QnActions::IDType actionType;
    std::map<QString, QString> actionParameters;
};

struct JoystickConfiguration 
{
    typedef QString ControlIdType;
    typedef QString OverrideNameType;

    QString configurationId;
    QString configurationName;
    std::map<ControlIdType, std::map<OverrideNameType, QString>> controlOverrides;
    std::map<ControlIdType, std::vector<Rule>> eventMapping;
};


struct Config
{
    typedef QString JoystickIdType;
    typedef QString ConfigurationIdType;

    std::map<JoystickIdType, ConfigurationIdType> enabledConfigurations;
    std::map<ConfigurationIdType, JoystickConfiguration> configurations;
};

const QString kPresetIndexParameterName = lit("presetIndex");

#define Rule_Fields (ruleId)(eventType)(actionType)(actionParameters)
#define JoystickConfiguration_Fields (configurationId)(configurationName)(controlOverrides)(eventMapping)
#define Config_Fields (enabledConfigurations)(configurations)

QN_FUSION_DECLARE_FUNCTIONS(nx::joystick::mapping::Rule, (json));
QN_FUSION_DECLARE_FUNCTIONS(nx::joystick::mapping::JoystickConfiguration, (json));
QN_FUSION_DECLARE_FUNCTIONS(nx::joystick::mapping::Config, (json));

} // namespace mapping
} // namespace joystick
} // namespace nx