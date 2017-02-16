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

QN_FUSION_DECLARE_FUNCTIONS(Rule, (metatype)(json))


struct JoystickConfiguration 
{
    typedef QString ControlIdType;
    typedef QString OverrideNameType;

    QString configurationId;
    QString configurationName;
    std::map<ControlIdType, std::map<OverrideNameType, QString>> controlOverrides;
    std::map<ControlIdType, std::vector<Rule>> eventMapping;
};

QN_FUSION_DECLARE_FUNCTIONS(JoystickConfiguration, (metatype)(json))

struct Config
{
    typedef QString JoystickIdType;
    typedef QString ConfigurationIdType;

    std::map<JoystickIdType, ConfigurationIdType> enabledConfigurations;
    std::map<ConfigurationIdType, JoystickConfiguration> configurations;
};

QN_FUSION_DECLARE_FUNCTIONS(Config, (metatype)(json))

const QString kPresetIndexParameterName = lit("presetIndex");

} // namespace mapping
} // namespace joystick
} // namespace nx