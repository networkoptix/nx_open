#pragma once

#include <ui/actions/action.h>
#include <plugins/io_device/joystick/joystick_common.h>

namespace nx {
namespace joystick {
namespace mapping {

struct Rule 
{
    Rule();
    Rule(
        QString id,
        nx::joystick::EventType condition,
        QnActions::IDType actType,
        QnActionParameters actParamteres):
        
        controlId(id),
        triggerCondition(condition),
        actionType(actType),
        actionParameters(actParamteres)
    {};

    QString controlId;
    nx::joystick::EventType triggerCondition;
    QnActions::IDType actionType;
    QnActionParameters actionParameters;
};

} // namespace mapping
} // namespace joystick
} // namespace nx