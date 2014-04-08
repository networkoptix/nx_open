#include "business_action_factory.h"

#include <business/actions/camera_output_business_action.h>
#include <business/actions/panic_business_action.h>
#include <business/actions/recording_business_action.h>
#include <business/actions/sendmail_business_action.h>
#include <business/actions/common_business_action.h>

QnAbstractBusinessActionPtr QnBusinessActionFactory::instantiateAction(const QnBusinessEventRulePtr &rule, const QnAbstractBusinessEventPtr &event, Qn::ToggleState state) {
    if (BusinessActionType::requiresCameraResource(rule->actionType()) && rule->actionResources().isEmpty())
        return QnAbstractBusinessActionPtr(); //camera is not exists anymore
    //TODO: #GDM check resource type?

    QnBusinessEventParameters runtimeParams = event->getRuntimeParams();

    QnAbstractBusinessActionPtr result = createAction(rule->actionType(), runtimeParams);

    result->setParams(rule->actionParams());
    result->setResources(rule->actionResources());

    if (BusinessEventType::hasToggleState(event->getEventType()) && BusinessActionType::hasToggleState(rule->actionType())) {
        Qn::ToggleState value = state != Qn::UndefinedState ? state : event->getToggleState();
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

QnAbstractBusinessActionPtr QnBusinessActionFactory::createAction(const BusinessActionType::Value actionType, const QnBusinessEventParameters &runtimeParams) {
    switch(actionType)
    {
        case BusinessActionType::NotDefined:
        case BusinessActionType::Bookmark:
        case BusinessActionType::Diagnostics:
        case BusinessActionType::ShowPopup:
        case BusinessActionType::PlaySound:
        case BusinessActionType::PlaySoundRepeated:
        case BusinessActionType::SayText:
            return QnAbstractBusinessActionPtr(new QnCommonBusinessAction(actionType, runtimeParams));

        case BusinessActionType::CameraOutput:       return QnAbstractBusinessActionPtr(new QnCameraOutputBusinessAction(false, runtimeParams));
        case BusinessActionType::CameraOutputInstant:return QnAbstractBusinessActionPtr(new QnCameraOutputBusinessAction(true, runtimeParams));
        case BusinessActionType::CameraRecording:    return QnAbstractBusinessActionPtr(new QnRecordingBusinessAction(runtimeParams));
        case BusinessActionType::PanicRecording:     return QnAbstractBusinessActionPtr(new QnPanicBusinessAction(runtimeParams));
        case BusinessActionType::SendMail:           return QnAbstractBusinessActionPtr(new QnSendMailBusinessAction(runtimeParams));
    }
    return QnAbstractBusinessActionPtr(new QnCommonBusinessAction(actionType, runtimeParams));
}
