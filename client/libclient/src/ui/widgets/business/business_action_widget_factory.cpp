#include "business_action_widget_factory.h"

#include <ui/widgets/business/empty_business_action_widget.h>
#include <ui/widgets/business/camera_output_business_action_widget.h>
#include <ui/widgets/business/recording_business_action_widget.h>
#include <ui/widgets/business/sendmail_business_action_widget.h>
#include <ui/widgets/business/popup_business_action_widget.h>
#include <ui/widgets/business/play_sound_business_action_widget.h>
#include <ui/widgets/business/say_text_business_action_widget.h>
#include <ui/widgets/business/bookmark_business_action_widget.h>
#include <ui/widgets/business/show_text_overlay_action_widget.h>
#include <ui/widgets/business/show_on_alarm_layout_action_widget.h>
#include <ui/widgets/business/ptz_preset_business_action_widget.h>
#include <ui/widgets/business/exec_http_request_action_widget.h>

QnAbstractBusinessParamsWidget* QnBusinessActionWidgetFactory::createWidget(QnBusiness::ActionType actionType, QWidget *parent) {
    switch (actionType) {
    case QnBusiness::CameraOutputAction:
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
    case QnBusiness::ExecutePtzPresetAction:
        return new QnExecPtzPresetBusinessActionWidget(parent);
    case QnBusiness::ShowTextOverlayAction:
        return new QnShowTextOverlayActionWidget(parent);
    case QnBusiness::ShowOnAlarmLayoutAction:
        return new QnShowOnAlarmLayoutActionWidget(parent);
    case QnBusiness::ExecHttpRequestAction:
        return new QnExecHttpRequestActionWidget(parent);
    default:
        break;
    }

    return new QnEmptyBusinessActionWidget(parent);
}
