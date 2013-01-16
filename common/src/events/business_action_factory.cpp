#include "business_action_factory.h"

#include <events/camera_output_business_action.h>
#include <events/panic_business_action.h>
#include <events/recording_business_action.h>
#include <events/sendmail_business_action.h>
#include <events/popup_business_action.h>
#include <events/common_business_action.h>

QnAbstractBusinessActionPtr QnBusinessActionFactory::createAction(const BusinessActionType::Value actionType, const QnBusinessParams &runtimeParams) {
    switch(actionType)
    {
        case BusinessActionType::BA_NotDefined:         return QnAbstractBusinessActionPtr(new QnCommonBusinessAction(actionType, runtimeParams));
        case BusinessActionType::BA_CameraOutput:       return QnAbstractBusinessActionPtr(new QnCameraOutputBusinessAction(runtimeParams));
        case BusinessActionType::BA_Bookmark:           return QnAbstractBusinessActionPtr(new QnCommonBusinessAction(actionType, runtimeParams));
        case BusinessActionType::BA_CameraRecording:    return QnAbstractBusinessActionPtr(new QnRecordingBusinessAction(runtimeParams));
        case BusinessActionType::BA_PanicRecording:     return QnAbstractBusinessActionPtr(new QnPanicBusinessAction(runtimeParams));
        case BusinessActionType::BA_SendMail:           return QnAbstractBusinessActionPtr(new QnSendMailBusinessAction(runtimeParams));
        case BusinessActionType::BA_Alert:              return QnAbstractBusinessActionPtr(new QnCommonBusinessAction(actionType, runtimeParams));
        case BusinessActionType::BA_ShowPopup:          return QnAbstractBusinessActionPtr(new QnPopupBusinessAction(runtimeParams));
    }
    return QnAbstractBusinessActionPtr(new QnCommonBusinessAction(actionType));
}

