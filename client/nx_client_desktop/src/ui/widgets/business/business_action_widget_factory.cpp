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

using namespace nx;

QnAbstractBusinessParamsWidget* QnBusinessActionWidgetFactory::createWidget(
    vms::event::ActionType actionType,
    QWidget* parent)
{
    switch (actionType)
    {
        case vms::event::CameraOutputAction:
            return new QnCameraOutputBusinessActionWidget(parent);
        case vms::event::CameraRecordingAction:
            return new QnRecordingBusinessActionWidget(parent);
        case vms::event::SendMailAction:
            return new QnSendmailBusinessActionWidget(parent);
        case vms::event::ShowPopupAction:
            return new QnPopupBusinessActionWidget(parent);
        case vms::event::PlaySoundAction:
        case vms::event::PlaySoundOnceAction:
            return new QnPlaySoundBusinessActionWidget(parent);
        case vms::event::SayTextAction:
            return new QnSayTextBusinessActionWidget(parent);
        case vms::event::BookmarkAction:
            return new QnBookmarkBusinessActionWidget(parent);
        case vms::event::ExecutePtzPresetAction:
            return new QnExecPtzPresetBusinessActionWidget(parent);
        case vms::event::ShowTextOverlayAction:
            return new QnShowTextOverlayActionWidget(parent);
        case vms::event::ShowOnAlarmLayoutAction:
            return new QnShowOnAlarmLayoutActionWidget(parent);
        case vms::event::ExecHttpRequestAction:
            return new QnExecHttpRequestActionWidget(parent);
        default:
            break;
    }

    return new QnEmptyBusinessActionWidget(parent);
}
