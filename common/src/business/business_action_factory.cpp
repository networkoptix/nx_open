#include "business_action_factory.h"

#include <common/common_module.h>

#include <business/actions/camera_output_business_action.h>
#include <business/actions/panic_business_action.h>
#include <business/actions/recording_business_action.h>
#include <business/actions/sendmail_business_action.h>
#include <business/actions/common_business_action.h>
#include <business/actions/bookmark_business_action.h>

#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>

QVector<QnUuid> toIdList(const QnResourceList& list)
{
    QVector<QnUuid> result;
    result.reserve(list.size());
    for (const QnResourcePtr& r: list)
        result << r->getId();
    return result;
}

QnAbstractBusinessActionPtr QnBusinessActionFactory::instantiateAction(
    const QnBusinessEventRulePtr &rule,
    const QnAbstractBusinessEventPtr &event,
    const QnUuid& moduleGuid,
    QnBusiness::EventState state)
{
    QnBusinessEventParameters runtimeParams = event->getRuntimeParamsEx(rule->eventParams());
    runtimeParams.sourceServerId = moduleGuid;

    QnAbstractBusinessActionPtr result = createAction(rule->actionType(), runtimeParams);

    result->setParams(rule->actionParams());
    result->setResources(rule->actionResources());

    if (QnBusiness::hasToggleState(event->getEventType()) && QnBusiness::hasToggleState(rule->actionType())) {
        QnBusiness::EventState value = state != QnBusiness::UndefinedState ? state : event->getToggleState();
        result->setToggleState(value);
    }
    result->setBusinessRuleId(rule->id());

    return result;
}

QnAbstractBusinessActionPtr QnBusinessActionFactory::instantiateAction(
    const QnBusinessEventRulePtr &rule,
    const QnAbstractBusinessEventPtr &event,
    const QnUuid& moduleGuid,
    const QnBusinessAggregationInfo &aggregationInfo)
{
    QnAbstractBusinessActionPtr result = instantiateAction(rule, event, moduleGuid);
    if (!result)
        return result;
    result->setAggregationCount(aggregationInfo.totalCount());

    if (QnSendMailBusinessActionPtr sendMailAction = result.dynamicCast<QnSendMailBusinessAction>())
        sendMailAction->setAggregationInfo(aggregationInfo);

    return result;
}

QnAbstractBusinessActionPtr QnBusinessActionFactory::createAction(const QnBusiness::ActionType actionType, const QnBusinessEventParameters &runtimeParams) {
    switch(actionType)
    {
        case QnBusiness::UndefinedAction:
        case QnBusiness::DiagnosticsAction:
        case QnBusiness::ShowPopupAction:
        case QnBusiness::PlaySoundOnceAction:
        case QnBusiness::PlaySoundAction:
        case QnBusiness::SayTextAction:
            break;

        case QnBusiness::CameraOutputAction:        return QnAbstractBusinessActionPtr(new QnCameraOutputBusinessAction(runtimeParams));
        case QnBusiness::CameraRecordingAction:     return QnAbstractBusinessActionPtr(new QnRecordingBusinessAction(runtimeParams));
        case QnBusiness::PanicRecordingAction:      return QnAbstractBusinessActionPtr(new QnPanicBusinessAction(runtimeParams));
        case QnBusiness::SendMailAction:            return QnAbstractBusinessActionPtr(new QnSendMailBusinessAction(runtimeParams));
        case QnBusiness::BookmarkAction:            return QnAbstractBusinessActionPtr(new QnBookmarkBusinessAction(runtimeParams));

        case QnBusiness::ExecutePtzPresetAction:
        case QnBusiness::ShowTextOverlayAction:
        case QnBusiness::ShowOnAlarmLayoutAction:
        case QnBusiness::ExecHttpRequestAction:
            break;

        default:
            NX_ASSERT(false, Q_FUNC_INFO, "All action types must be handled.");
            break;
    }

    return QnAbstractBusinessActionPtr(new QnCommonBusinessAction(actionType, runtimeParams));
}

QnAbstractBusinessActionPtr QnBusinessActionFactory::cloneAction(QnAbstractBusinessActionPtr action)
{
    QnAbstractBusinessActionPtr result = createAction(action->actionType(), action->getRuntimeParams());
    result->assign(*action);
    return result;
}
