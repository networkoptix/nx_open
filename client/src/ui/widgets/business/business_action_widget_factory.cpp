#include "business_action_widget_factory.h"

#include <ui/widgets/business/empty_business_action_widget.h>
#include <ui/widgets/business/camera_output_business_action_widget.h>
#include <ui/widgets/business/recording_business_action_widget.h>
#include <ui/widgets/business/sendmail_business_action_widget.h>
#include <ui/widgets/business/popup_business_action_widget.h>
#include <ui/widgets/business/play_sound_business_action_widget.h>

QnAbstractBusinessParamsWidget* QnBusinessActionWidgetFactory::createWidget(BusinessActionType::Value actionType, QWidget *parent) {
    switch (actionType) {
    case BusinessActionType::CameraOutput:
    case BusinessActionType::CameraOutputInstant:
        return new QnCameraOutputBusinessActionWidget(parent);
    case BusinessActionType::CameraRecording:
        return new QnRecordingBusinessActionWidget(parent);
    case BusinessActionType::SendMail:
        return new QnSendmailBusinessActionWidget(parent);
    case BusinessActionType::ShowPopup:
        return new QnPopupBusinessActionWidget(parent);
    case BusinessActionType::PlaySound:
        return new QnPlaySoundBusinessActionWidget(parent);
    default:
        return new QnEmptyBusinessActionWidget(parent);
    }
}
