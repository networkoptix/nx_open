#include "business_action_factory.h"

#include <events/camera_output_business_action.h>
#include <events/panic_business_action.h>
#include <events/recording_business_action.h>
#include <events/sendmail_business_action.h>
#include <events/common_business_action.h>

QnAbstractBusinessActionPtr QnBusinessActionFactory::createAction(BusinessActionType::Value actionType, QnBusinessParams runtimeParams) {
    switch(actionType)
    {
        case BusinessActionType::BA_NotDefined:         return QnAbstractBusinessActionPtr(new QnCommonBusinessAction(actionType));
        case BusinessActionType::BA_CameraOutput:       return QnAbstractBusinessActionPtr(new QnCameraOutputBusinessAction());
        case BusinessActionType::BA_Bookmark:           return QnAbstractBusinessActionPtr(new QnCommonBusinessAction(actionType));
        case BusinessActionType::BA_CameraRecording:    return QnAbstractBusinessActionPtr(new QnRecordingBusinessAction());
        case BusinessActionType::BA_PanicRecording:     return QnAbstractBusinessActionPtr(new QnPanicBusinessAction());
        case BusinessActionType::BA_SendMail:           return QnAbstractBusinessActionPtr(new QnSendMailBusinessAction(runtimeParams));
        case BusinessActionType::BA_Alert:              return QnAbstractBusinessActionPtr(new QnCommonBusinessAction(actionType));
        case BusinessActionType::BA_ShowPopup:          return QnAbstractBusinessActionPtr(new QnCommonBusinessAction(actionType));
    }
    return QnAbstractBusinessActionPtr(new QnCommonBusinessAction(actionType));
}

