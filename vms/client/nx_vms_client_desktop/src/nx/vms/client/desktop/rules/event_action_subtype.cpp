// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_action_subtype.h"

#include <nx/utils/log/assert.h>

using namespace nx::vms::api;

namespace nx::vms::client::desktop {

EventSubtype eventSubtype(nx::vms::api::EventType event)
{
    switch (event)
    {
        case EventType::cameraMotionEvent:
        case EventType::cameraInputEvent:
        case EventType::softwareTriggerEvent:
        case EventType::analyticsSdkEvent:
        case EventType::analyticsSdkObjectDetected:
        case EventType::userDefinedEvent:
        case EventType::pluginDiagnosticEvent:
            return EventSubtype::user;

        case EventType::cameraDisconnectEvent:
        case EventType::storageFailureEvent:
        case EventType::networkIssueEvent:
        case EventType::cameraIpConflictEvent:
        case EventType::serverFailureEvent:
        case EventType::serverConflictEvent:
        case EventType::ldapSyncIssueEvent:
        case EventType::licenseIssueEvent:
        case EventType::poeOverBudgetEvent:
        case EventType::fanErrorEvent:
        case EventType::serverCertificateError:
            return EventSubtype::failure;

        case EventType::serverStartEvent:
        case EventType::backupFinishedEvent:
            return EventSubtype::success;

        default:
            NX_ASSERT(false, "Event should belong to a certain subtype");
            return EventSubtype::undefined;
    }
}

ActionSubtype actionSubtype(nx::vms::api::ActionType actionType)
{
    // #vbreus Categorization has been moved as is from QnBusinessTypesComparator.
    switch (actionType)
    {
        case ActionType::openLayoutAction:
        case ActionType::showOnAlarmLayoutAction:
        case ActionType::fullscreenCameraAction:
        case ActionType::exitFullscreenAction:
            return ActionSubtype::client;

        case ActionType::cameraOutputAction:
        case ActionType::bookmarkAction:
        case ActionType::cameraRecordingAction:
        case ActionType::panicRecordingAction:
        case ActionType::sendMailAction:
        case ActionType::diagnosticsAction:
        case ActionType::showPopupAction:
        case ActionType::pushNotificationAction:
        case ActionType::playSoundAction:
        case ActionType::playSoundOnceAction:
        case ActionType::sayTextAction:
        case ActionType::executePtzPresetAction:
        case ActionType::showTextOverlayAction:
        case ActionType::execHttpRequestAction:
        case ActionType::buzzerAction:
        case ActionType::acknowledgeAction:
            return ActionSubtype::server;

        default:
            NX_ASSERT(false, "Action should belong to a certain subtype");
            return ActionSubtype::undefined;
    }
}

QList<EventType> filterEventsBySubtype(const QList<EventType>& events, EventSubtype subtype)
{
    QList<EventType> result;

    std::copy_if(events.cbegin(), events.cend(), std::back_inserter(result),
        [subtype](EventType eventType) { return eventSubtype(eventType) == subtype; });

    return result;
}

QList<ActionType> filterActionsBySubtype(const QList<ActionType>& events, ActionSubtype subtype)
{
    QList<ActionType> result;

    std::copy_if(events.cbegin(), events.cend(), std::back_inserter(result),
        [subtype](ActionType actionType) { return actionSubtype(actionType) == subtype; });

    return result;
}

} // namespace nx::vms::client::desktop
