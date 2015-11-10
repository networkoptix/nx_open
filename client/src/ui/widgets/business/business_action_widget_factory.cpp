#include "business_action_widget_factory.h"

#include <ui/widgets/business/empty_business_action_widget.h>
#include <ui/widgets/business/camera_output_business_action_widget.h>
#include <ui/widgets/business/recording_business_action_widget.h>
#include <ui/widgets/business/sendmail_business_action_widget.h>
#include <ui/widgets/business/popup_business_action_widget.h>
#include <ui/widgets/business/play_sound_business_action_widget.h>
#include <ui/widgets/business/say_text_business_action_widget.h>
#include <ui/widgets/business/bookmark_business_action_widget.h>

QnAbstractBusinessParamsWidget* QnBusinessActionWidgetFactory::createWidget(QnBusiness::ActionType actionType, QWidget *parent) {
    switch (actionType) {
    case QnBusiness::CameraOutputAction:
    case QnBusiness::CameraOutputOnceAction:
        return new QnCameraOutputBusinessActionWidget(parent);
    case QnBusiness::CameraRecordingAction:
        return new QnRecordingBusinessActionWidget(parent);
    case QnBusiness::SendMailAction:
        return new QnSendmailBusinessActionWidget(parent);
    case QnBusiness::ShowPopupAction:
        return new QnPopupBusinessActionWidget(parent);
    case QnBusiness::PlaySoundAction:
    case QnBusiness::PlaySoundOnceAction:
        return new QnPlaySoundBusinessActionWidget(parent);
    case QnBusiness::SayTextAction:
        return new QnSayTextBusinessActionWidget(parent);
    case QnBusiness::BookmarkAction:
        return new QnBookmarkBusinessActionWidget(parent);
    default:
        return new QnEmptyBusinessActionWidget(parent);
    }
}
