// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "level.h"

#include "actions/abstract_action.h"

namespace nx::vms::event {

Level levelOf(const AbstractActionPtr& action)
{
    if (action->actionType() == ActionType::playSoundAction)
        return Level::common;

    if (action->actionType() == ActionType::showOnAlarmLayoutAction)
        return Level::critical;

    if (action->actionType() == ActionType::showIntercomInformer)
        return Level::success;

    return levelOf(action->getRuntimeParams());
}

Level levelOf(const EventParameters& params)
{
    EventType eventType = params.eventType;

    if (eventType >= EventType::userDefinedEvent)
        return Level::common;

    switch (eventType)
    {
        // Gray notifications.
        case EventType::cameraMotionEvent:
        case EventType::cameraInputEvent:
        case EventType::serverStartEvent:
        case EventType::softwareTriggerEvent:
        case EventType::analyticsSdkEvent:
        case EventType::analyticsSdkObjectDetected:
            return Level::common;

        // Yellow notifications.
        case EventType::ldapSyncIssueEvent:
        case EventType::networkIssueEvent:
        case EventType::cameraIpConflictEvent:
        case EventType::serverConflictEvent:
            return Level::important;

        // Red notifications.
        case EventType::cameraDisconnectEvent:
        case EventType::storageFailureEvent:
        case EventType::serverFailureEvent:
        case EventType::licenseIssueEvent:
        case EventType::fanErrorEvent:
        case EventType::poeOverBudgetEvent:
        case EventType::serverCertificateError:
            return Level::critical;

        case EventType::backupFinishedEvent:
            NX_ASSERT(false, "This event is deprecated");
            return Level::none;

        case EventType::pluginDiagnosticEvent:
        {
            using namespace nx::vms::api;
            switch (params.metadata.level)
            {
                case EventLevel::error:
                    return Level::critical;

                case EventLevel::warning:
                    return Level::important;

                default:
                    return Level::common;
            }
        }

        default:
            NX_ASSERT(false, "All enum values must be handled");
            return Level::none;
    }
}

} // namespace nx::vms::event
