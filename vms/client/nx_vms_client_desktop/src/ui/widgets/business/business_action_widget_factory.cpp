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
#include <ui/widgets/business/open_layout_action_widget.h>
#include <ui/widgets/business/fullscreen_camera_action_widget.h>
#include <ui/widgets/business/exit_fullscreen_action_widget.h>

using namespace nx;

QnAbstractBusinessParamsWidget* QnBusinessActionWidgetFactory::createWidget(
    vms::api::ActionType actionType,
    QWidget* parent)
{
    switch (actionType)
    {
        case vms::api::ActionType::cameraOutputAction:
            return new QnCameraOutputBusinessActionWidget(parent);
        case vms::api::ActionType::cameraRecordingAction:
            return new QnRecordingBusinessActionWidget(parent);
        case vms::api::ActionType::sendMailAction:
            return new QnSendmailBusinessActionWidget(parent);
        case vms::api::ActionType::showPopupAction:
            return new QnPopupBusinessActionWidget(parent);
        case vms::api::ActionType::playSoundAction:
        case vms::api::ActionType::playSoundOnceAction:
            return new QnPlaySoundBusinessActionWidget(parent);
        case vms::api::ActionType::sayTextAction:
            return new QnSayTextBusinessActionWidget(parent);
        case vms::api::ActionType::bookmarkAction:
            return new QnBookmarkBusinessActionWidget(parent);
        case vms::api::ActionType::executePtzPresetAction:
            return new QnExecPtzPresetBusinessActionWidget(parent);
        case vms::api::ActionType::showTextOverlayAction:
            return new QnShowTextOverlayActionWidget(parent);
        case vms::api::ActionType::showOnAlarmLayoutAction:
            return new QnShowOnAlarmLayoutActionWidget(parent);
        case vms::api::ActionType::execHttpRequestAction:
            return new QnExecHttpRequestActionWidget(parent);
        case vms::api::ActionType::openLayoutAction:
            return new nx::vms::client::desktop::OpenLayoutActionWidget(parent);
        case vms::api::ActionType::fullscreenCameraAction:
            return new QnFullscreenCameraActionWidget(parent);
        case vms::api::ActionType::exitFullscreenAction:
            return new QnExitFullscreenActionWidget(parent);

        default:
            break;
    }

    return new QnEmptyBusinessActionWidget(actionType, parent);
}
