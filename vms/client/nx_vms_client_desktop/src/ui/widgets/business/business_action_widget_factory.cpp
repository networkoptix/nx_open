// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "business_action_widget_factory.h"

#include <nx/vms/client/desktop/window_context.h>
#include <ui/widgets/business/bookmark_business_action_widget.h>
#include <ui/widgets/business/buzzer_business_action_widget.h>
#include <ui/widgets/business/camera_output_business_action_widget.h>
#include <ui/widgets/business/empty_business_action_widget.h>
#include <ui/widgets/business/exec_http_request_action_widget.h>
#include <ui/widgets/business/exit_fullscreen_action_widget.h>
#include <ui/widgets/business/fullscreen_camera_action_widget.h>
#include <ui/widgets/business/open_layout_action_widget.h>
#include <ui/widgets/business/play_sound_business_action_widget.h>
#include <ui/widgets/business/popup_business_action_widget.h>
#include <ui/widgets/business/ptz_preset_business_action_widget.h>
#include <ui/widgets/business/push_notification_business_action_widget.h>
#include <ui/widgets/business/recording_business_action_widget.h>
#include <ui/widgets/business/say_text_business_action_widget.h>
#include <ui/widgets/business/sendmail_business_action_widget.h>
#include <ui/widgets/business/show_on_alarm_layout_action_widget.h>
#include <ui/widgets/business/show_text_overlay_action_widget.h>

using namespace nx;
using namespace nx::vms::client::desktop;

QnAbstractBusinessParamsWidget* QnBusinessActionWidgetFactory::createWidget(
    vms::api::ActionType actionType,
    WindowContext* context,
    QWidget* parent)
{
    SystemContext* systemContext = context->system();

    switch (actionType)
    {
        case vms::api::ActionType::cameraOutputAction:
            return new QnCameraOutputBusinessActionWidget(systemContext, parent);
        case vms::api::ActionType::cameraRecordingAction:
            return new QnRecordingBusinessActionWidget(systemContext, parent);
        case vms::api::ActionType::sendMailAction:
            return new QnSendmailBusinessActionWidget(context, parent);
        case vms::api::ActionType::showPopupAction:
            return new QnPopupBusinessActionWidget(systemContext, parent);
        case vms::api::ActionType::pushNotificationAction:
            return new PushNotificationBusinessActionWidget(context, parent);
        case vms::api::ActionType::playSoundAction:
        case vms::api::ActionType::playSoundOnceAction:
            return new QnPlaySoundBusinessActionWidget(context, parent);
        case vms::api::ActionType::sayTextAction:
            return new QnSayTextBusinessActionWidget(systemContext, parent);
        case vms::api::ActionType::bookmarkAction:
            return new QnBookmarkBusinessActionWidget(systemContext, parent);
        case vms::api::ActionType::executePtzPresetAction:
            return new QnExecPtzPresetBusinessActionWidget(systemContext, parent);
        case vms::api::ActionType::showTextOverlayAction:
            return new QnShowTextOverlayActionWidget(systemContext, parent);
        case vms::api::ActionType::showOnAlarmLayoutAction:
            return new QnShowOnAlarmLayoutActionWidget(systemContext, parent);
        case vms::api::ActionType::execHttpRequestAction:
            return new QnExecHttpRequestActionWidget(systemContext, parent);
        case vms::api::ActionType::openLayoutAction:
            return new OpenLayoutActionWidget(systemContext, parent);
        case vms::api::ActionType::fullscreenCameraAction:
            return new QnFullscreenCameraActionWidget(systemContext, parent);
        case vms::api::ActionType::exitFullscreenAction:
            return new QnExitFullscreenActionWidget(systemContext, parent);
        case vms::api::ActionType::buzzerAction:
            return new BuzzerBusinessActionWidget(systemContext, parent);

        default:
            break;
    }

    return new QnEmptyBusinessActionWidget(systemContext, actionType, parent);
}
