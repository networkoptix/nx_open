// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "action_factory.h"

#include <nx/vms/common/system_context.h>
#include <nx/vms/event/actions/actions.h>
#include <nx/vms/event/events/ldap_sync_issue_event.h>
#include <nx/vms/event/rule.h>

using namespace nx::vms::common;

namespace nx {
namespace vms {
namespace event {

AbstractActionPtr ActionFactory::createAction(
    const ActionType actionType,
    const EventParameters& runtimeParams)
{
    switch (actionType)
    {
        case ActionType::cameraOutputAction:
            return AbstractActionPtr(new CameraOutputAction(runtimeParams));
        case ActionType::cameraRecordingAction:
            return AbstractActionPtr(new RecordingAction(runtimeParams));
        case ActionType::panicRecordingAction:
            return AbstractActionPtr(new PanicAction(runtimeParams));
        case ActionType::sendMailAction:
            return AbstractActionPtr(new SendMailAction(runtimeParams));
        case ActionType::bookmarkAction:
            return AbstractActionPtr(new BookmarkAction(runtimeParams));

        case ActionType::undefinedAction:
        case ActionType::diagnosticsAction:
        case ActionType::showPopupAction:
        case ActionType::pushNotificationAction:
        case ActionType::playSoundOnceAction:
        case ActionType::playSoundAction:
        case ActionType::sayTextAction:
        case ActionType::executePtzPresetAction:
        case ActionType::showTextOverlayAction:
        case ActionType::showOnAlarmLayoutAction:
        case ActionType::execHttpRequestAction:
        case ActionType::openLayoutAction:
        case ActionType::fullscreenCameraAction:
        case ActionType::exitFullscreenAction:
        case ActionType::buzzerAction:
            return AbstractActionPtr(new CommonAction(actionType, runtimeParams));

        default:
            NX_ASSERT(false, "Unexpected action type: %1", actionType);
            return AbstractActionPtr(new CommonAction(actionType, runtimeParams));
    }
}

} // namespace event
} // namespace vms
} // namespace nx
