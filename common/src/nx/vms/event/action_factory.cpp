#include "action_factory.h"

#include <common/common_module.h>

#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/vms/event/actions/actions.h>

namespace nx {
namespace vms {
namespace event {

QVector<QnUuid> toIdList(const QnResourceList& list)
{
    QVector<QnUuid> result;
    result.reserve(list.size());
    for (const QnResourcePtr& r: list)
        result << r->getId();
    return result;
}

AbstractActionPtr ActionFactory::instantiateAction(
    const RulePtr& rule,
    const AbstractEventPtr& event,
    const QnUuid& moduleGuid,
    EventState state)
{
    EventParameters runtimeParams = event->getRuntimeParamsEx(rule->eventParams());
    runtimeParams.sourceServerId = moduleGuid;

    AbstractActionPtr result = createAction(rule->actionType(), runtimeParams);

    result->setParams(rule->actionParams());
    result->setResources(rule->actionResources());

    if (hasToggleState(event->getEventType()) && hasToggleState(rule->actionType()))
    {
        EventState value = state != EventState::undefined ? state : event->getToggleState();
        result->setToggleState(value);
    }

    result->setRuleId(rule->id());
    return result;
}

AbstractActionPtr ActionFactory::instantiateAction(
    const RulePtr& rule,
    const AbstractEventPtr& event,
    const QnUuid& moduleGuid,
    const AggregationInfo& aggregationInfo)
{
    AbstractActionPtr result = instantiateAction(rule, event, moduleGuid);
    if (!result)
        return result;

    result->setAggregationCount(aggregationInfo.totalCount());

    if (auto sendMailAction = result.dynamicCast<class SendMailAction>())
        sendMailAction->setAggregationInfo(aggregationInfo);

    return result;
}

AbstractActionPtr ActionFactory::createAction(
    const ActionType actionType,
    const EventParameters& runtimeParams)
{
    switch (actionType)
    {
        case cameraOutputAction:
            return AbstractActionPtr(new CameraOutputAction(runtimeParams));
        case cameraRecordingAction:
            return AbstractActionPtr(new RecordingAction(runtimeParams));
        case panicRecordingAction:
            return AbstractActionPtr(new PanicAction(runtimeParams));
        case sendMailAction:
            return AbstractActionPtr(new SendMailAction(runtimeParams));
        case bookmarkAction:
            return AbstractActionPtr(new BookmarkAction(runtimeParams));

        case undefinedAction:
        case diagnosticsAction:
        case showPopupAction:
        case playSoundOnceAction:
        case playSoundAction:
        case sayTextAction:
        case executePtzPresetAction:
        case showTextOverlayAction:
        case showOnAlarmLayoutAction:
        case execHttpRequestAction:
        case openLayoutAction:
            return AbstractActionPtr(new CommonAction(actionType, runtimeParams));

        default:
            NX_ASSERT(false, Q_FUNC_INFO, "All action types must be handled.");
            return AbstractActionPtr(new CommonAction(actionType, runtimeParams));
    }
}

AbstractActionPtr ActionFactory::cloneAction(AbstractActionPtr action)
{
    AbstractActionPtr result = createAction(action->actionType(), action->getRuntimeParams());
    result->assign(*action);
    return result;
}

} // namespace event
} // namespace vms
} // namespace nx
