#include "broadcast_action_handler.h"

#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/actions/common_action.h>
#include <nx/mediaserver/event/rule_processor.h>

namespace {

static const auto kActionTypeTag = lit("actionType");
static const auto kActionParametersTag = lit("actionParameters");

} // namespace

namespace nx {
namespace mediaserver {
namespace rest {

int BroadcastActionHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParamList& params,
    QByteArray& /*result*/,
    QByteArray& /*contentType*/,
    const QnRestConnectionProcessor* /*processor*/)
{
    static const int kInvalidParametersAnswer = 422; //< Invalid parameters error code. TODO: CHANGE FOR CONSTANT BEFORE REVIEW

    const auto itType = params.find(kActionTypeTag);
    if (itType == params.end())
        return kInvalidParametersAnswer;

    const auto itParams = params.find(kActionParametersTag);
    if (itParams == params.end())
        return kInvalidParametersAnswer;

    vms::event::ActionParameters actionParameters;
    if (!QJson::deserialize(itParams->second, &actionParameters))
        return kInvalidParametersAnswer;

    auto actionType = vms::event::ActionType::undefinedAction;
    if (!QnLexical::deserialize(itType->second, &actionType))
        return kInvalidParametersAnswer;

    const auto action = vms::event::CommonAction::createBroadcastAction(
        actionType, actionParameters);

    if (!qnEventRuleProcessor->broadcastAction(action))
        return 200; //< TODO: change to constant before review

    qDebug() << "Can't send event";
    return 503; // TODO change to constant;
}
}

}
}

