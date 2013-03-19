#include "business_action_factory.h"

#include <business/actions/camera_output_business_action.h>
#include <business/actions/panic_business_action.h>
#include <business/actions/recording_business_action.h>
#include <business/actions/sendmail_business_action.h>
#include <business/actions/popup_business_action.h>
#include <business/actions/common_business_action.h>

QnAbstractBusinessActionPtr QnBusinessActionFactory::createAction(const BusinessActionType::Value actionType, const QnBusinessParams &runtimeParams) {
    switch(actionType)
    {
        case BusinessActionType::NotDefined:         return QnAbstractBusinessActionPtr(new QnCommonBusinessAction(actionType, runtimeParams));
        case BusinessActionType::CameraOutput:       return QnAbstractBusinessActionPtr(new QnCameraOutputBusinessAction(runtimeParams));
        case BusinessActionType::Bookmark:           return QnAbstractBusinessActionPtr(new QnCommonBusinessAction(actionType, runtimeParams));
        case BusinessActionType::CameraRecording:    return QnAbstractBusinessActionPtr(new QnRecordingBusinessAction(runtimeParams));
        case BusinessActionType::PanicRecording:     return QnAbstractBusinessActionPtr(new QnPanicBusinessAction(runtimeParams));
        case BusinessActionType::SendMail:           return QnAbstractBusinessActionPtr(new QnSendMailBusinessAction(runtimeParams));
        case BusinessActionType::Alert:              return QnAbstractBusinessActionPtr(new QnCommonBusinessAction(actionType, runtimeParams));
        case BusinessActionType::ShowPopup:          return QnAbstractBusinessActionPtr(new QnPopupBusinessAction(runtimeParams));
    }
    return QnAbstractBusinessActionPtr(new QnCommonBusinessAction(actionType, runtimeParams));
}

