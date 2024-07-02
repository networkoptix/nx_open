// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "action_factory.h"

#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/event/actions/actions.h>

using namespace nx::vms::common;

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
    SystemContext* systemContext,
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

    if (hasToggleState(event->getEventType(), runtimeParams, systemContext) &&
        hasToggleState(rule->actionType()))
    {
        EventState value = state != EventState::undefined ? state : event->getToggleState();
        result->setToggleState(value);
    }

    result->setRuleId(rule->id());
    return result;
}

AbstractActionPtr ActionFactory::instantiateAction(
    SystemContext* systemContext,
    const RulePtr& rule,
    const AbstractEventPtr& event,
    const QnUuid& moduleGuid,
    const AggregationInfo& aggregationInfo)
{
    AbstractActionPtr result = instantiateAction(systemContext, rule, event, moduleGuid);
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
        case ActionType::cameraOutputAction:
            return AbstractActionPtr(new CameraOutputAction(runtimeParams));
        case ActionType::cameraRecordingAction:
            return AbstractActionPtr(new RecordingAction(runtimeParams));
        case ActionType::panicRecordingAction:
            return AbstractActionPtr(new PanicAction(runtimeParams));
        case ActionType::sendMailAction:
            return AbstractActionPtr(new SendMailAction(runtimeParams));
        case ActionType::bookmarkAction:
            return AbstractActionPtr(new BookmarkAction(runtimeParams));
        case ActionType::showIntercomInformer:
            return AbstractActionPtr(new CommonAction(actionType, runtimeParams));

        case ActionType::undefinedAction:
        case ActionType::diagnosticsAction:
        case ActionType::showPopupAction:
        case ActionType::pushNotificationAction:
        case ActionType::playSoundOnceAction:
        case ActionType::playSoundAction:
        case ActionType::sayTextAction:
        case ActionType::executePtzPresetAction:
        case ActionType::showTextOverlayAction:
        case ActionType::showOnAlarmLayoutAction:
        case ActionType::execHttpRequestAction:
        case ActionType::openLayoutAction:
        case ActionType::fullscreenCameraAction:
        case ActionType::exitFullscreenAction:
        case ActionType::buzzerAction:
        case ActionType::vxMonitoringAction:
            return AbstractActionPtr(new CommonAction(actionType, runtimeParams));

        default:
            NX_ASSERT(false, "All action types must be handled.");
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
