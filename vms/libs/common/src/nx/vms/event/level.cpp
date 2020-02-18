#include "level.h"

#include "actions/abstract_action.h"

namespace nx::vms::event {

Level levelOf(const AbstractActionPtr& action)
{
    if (action->actionType() == ActionType::playSoundAction)
        return Level::common;

    if (action->actionType() == ActionType::showOnAlarmLayoutAction)
        return Level::critical;

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
            return Level::common;

        // Yellow notifications.
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
            return Level::critical;

        case EventType::backupFinishedEvent:
        {
            EventReason reason = static_cast<EventReason>(params.reasonCode);
            const bool failure =
                reason == EventReason::backupFailedChunkError ||
                reason == EventReason::backupFailedNoBackupStorageError ||
                reason == EventReason::backupFailedSourceFileError ||
                reason == EventReason::backupFailedSourceStorageError ||
                reason == EventReason::backupFailedTargetFileError;

            if (failure)
                return Level::critical;

            const bool success = reason == EventReason::backupDone;
            return success ? Level::success : Level::common;
        }

        case EventType::pluginDiagnosticEvent:
        {
            using namespace nx::vms::api;
            switch (params.metadata.level)
            {
                case EventLevel::ErrorEventLevel:
                    return Level::critical;

                case EventLevel::WarningEventLevel:
                    return Level::important;

                default:
                    return Level::common;
            }
        }

        default:
            NX_ASSERT(false, "All enum values must be handled");
            return Level::no;
    }
}

} // namespace nx::vms::event
