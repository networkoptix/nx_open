#include "business_action_factory.h"

#include <business/actions/camera_output_business_action.h>
#include <business/actions/panic_business_action.h>
#include <business/actions/recording_business_action.h>
#include <business/actions/sendmail_business_action.h>
#include <business/actions/common_business_action.h>

QnAbstractBusinessActionPtr QnBusinessActionFactory::instantiateAction(const QnBusinessEventRulePtr &rule, const QnAbstractBusinessEventPtr &event, QnBusiness::EventState state) {
    if (QnBusiness::requiresCameraResource(rule->actionType()) && rule->actionResources().isEmpty())
        return QnAbstractBusinessActionPtr(); //camera is not exists anymore
    //TODO: #GDM #Business check resource type?

    QnBusinessEventParameters runtimeParams = event->getRuntimeParams();

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

QnAbstractBusinessActionPtr QnBusinessActionFactory::instantiateAction(const QnBusinessEventRulePtr &rule,
                                                                       const QnAbstractBusinessEventPtr &event,
                                                                       const QnBusinessAggregationInfo &aggregationInfo) {
    QnAbstractBusinessActionPtr result = instantiateAction(rule, event);
    if (!result)
        return result;
    result->setAggregationCount(aggregationInfo.totalCount());

    if (QnSendMailBusinessActionPtr sendMailAction = result.dynamicCast<QnSendMailBusinessAction>()) {
        sendMailAction->setAggregationInfo(aggregationInfo);
    }

    return result;
}

QnAbstractBusinessActionPtr QnBusinessActionFactory::createAction(const QnBusiness::ActionType actionType, const QnBusinessEventParameters &runtimeParams) {
    switch(actionType)
    {
        case QnBusiness::UndefinedAction:
        case QnBusiness::BookmarkAction:
        case QnBusiness::DiagnosticsAction:
        case QnBusiness::ShowPopupAction:
        case QnBusiness::PlaySoundOnceAction:
        case QnBusiness::PlaySoundAction:
        case QnBusiness::SayTextAction:
            return QnAbstractBusinessActionPtr(new QnCommonBusinessAction(actionType, runtimeParams));

        case QnBusiness::CameraOutputAction:       return QnAbstractBusinessActionPtr(new QnCameraOutputBusinessAction(false, runtimeParams));
        case QnBusiness::CameraOutputOnceAction:return QnAbstractBusinessActionPtr(new QnCameraOutputBusinessAction(true, runtimeParams));
        case QnBusiness::CameraRecordingAction:    return QnAbstractBusinessActionPtr(new QnRecordingBusinessAction(runtimeParams));
        case QnBusiness::PanicRecordingAction:     return QnAbstractBusinessActionPtr(new QnPanicBusinessAction(runtimeParams));
        case QnBusiness::SendMailAction:           return QnAbstractBusinessActionPtr(new QnSendMailBusinessAction(runtimeParams));
        default: return QnAbstractBusinessActionPtr(new QnCommonBusinessAction(actionType, runtimeParams));
    }
}
