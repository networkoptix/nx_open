#include "business_action_widget_factory.h"

#include <ui/widgets/business/empty_business_action_widget.h>
#include <ui/widgets/business/camera_output_business_action_widget.h>
#include <ui/widgets/business/recording_business_action_widget.h>
#include <ui/widgets/business/sendmail_business_action_widget.h>
#include <ui/widgets/business/popup_business_action_widget.h>

QnAbstractBusinessParamsWidget* QnBusinessActionWidgetFactory::createWidget(BusinessActionType::Value actionType, QWidget *parent,
                                                                            QnWorkbenchContext *context) {
    switch (actionType) {
        case BusinessActionType::CameraOutput:
            return new QnCameraOutputBusinessActionWidget(parent);
        case BusinessActionType::CameraRecording:
            return new QnRecordingBusinessActionWidget(parent);
        case BusinessActionType::SendMail:
            return new QnSendmailBusinessActionWidget(parent, context);
        case BusinessActionType::ShowPopup:
            return new QnPopupBusinessActionWidget(parent, context);
        default:
            return new QnEmptyBusinessActionWidget(parent);
    }
}
