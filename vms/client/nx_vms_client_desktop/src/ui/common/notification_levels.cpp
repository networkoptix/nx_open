#include "notification_levels.h"

#include <nx/vms/event/actions/abstract_action.h>

#include <utils/common/warnings.h>

#include <ui/style/globals.h>

using namespace nx;

using nx::vms::api::EventType;
using nx::vms::api::ActionType;

QnNotificationLevel::Value QnNotificationLevel::valueOf(const vms::event::AbstractActionPtr& action)
{
    if (action->actionType() == ActionType::playSoundAction)
        return Value::CommonNotification;

    if (action->actionType() == ActionType::showOnAlarmLayoutAction)
        return Value::CriticalNotification;

    return valueOf(action->getRuntimeParams());
}

QnNotificationLevel::Value QnNotificationLevel::valueOf(
    const nx::vms::event::EventParameters& params)
{
    EventType eventType = params.eventType;

    if (eventType >= EventType::userDefinedEvent)
        return Value::CommonNotification;

    switch (eventType)
    {
        // Gray notifications.
        case EventType::cameraMotionEvent:
        case EventType::cameraInputEvent:
        case EventType::serverStartEvent:
        case EventType::softwareTriggerEvent:
        case EventType::analyticsSdkEvent:
            return Value::CommonNotification;

        // Yellow notifications.
        case EventType::networkIssueEvent:
        case EventType::cameraIpConflictEvent:
        case EventType::serverConflictEvent:
            return Value::ImportantNotification;

        // Red notifications.
        case EventType::cameraDisconnectEvent:
        case EventType::storageFailureEvent:
        case EventType::serverFailureEvent:
        case EventType::licenseIssueEvent:
            return Value::CriticalNotification;

        case EventType::backupFinishedEvent:
        {
            vms::api::EventReason reason = static_cast<vms::api::EventReason>(params.reasonCode);
            const bool failure =
                reason == vms::api::EventReason::backupFailedChunkError ||
                reason == vms::api::EventReason::backupFailedNoBackupStorageError ||
                reason == vms::api::EventReason::backupFailedSourceFileError ||
                reason == vms::api::EventReason::backupFailedSourceStorageError ||
                reason == vms::api::EventReason::backupFailedTargetFileError;

            if (failure)
                return Value::CriticalNotification;

            const bool success = reason == vms::api::EventReason::backupDone;
            return success ? Value::SuccessNotification : Value::CommonNotification;
        }

        case EventType::pluginEvent:
        {
            using namespace nx::vms::api;
            switch (params.metadata.level)
            {
                case EventLevel::ErrorEventLevel:
                    return Value::CriticalNotification;

                case EventLevel::WarningEventLevel:
                    return Value::ImportantNotification;

                default:
                    return Value::CommonNotification;
            }
        }

        default:
            NX_ASSERT(false, "All enum values must be handled");
            return Value::NoNotification;
    }
}

QnNotificationLevel::Value QnNotificationLevel::valueOf(QnSystemHealth::MessageType messageType)
{
    switch (messageType)
    {
        case QnSystemHealth::CloudPromo:
            return QnNotificationLevel::Value::OtherNotification;

        // Gray notifications.
        case QnSystemHealth::ArchiveRebuildCanceled:
        case QnSystemHealth::RemoteArchiveSyncStarted:
        case QnSystemHealth::RemoteArchiveSyncProgress:
            return QnNotificationLevel::Value::CommonNotification;

        // Green notifications.
        case QnSystemHealth::ArchiveRebuildFinished:
        case QnSystemHealth::RemoteArchiveSyncFinished:
        case QnSystemHealth::ArchiveFastScanFinished: //< This one is never displayed though.
            return QnNotificationLevel::Value::SuccessNotification;

        // Yellow notifications.
        case QnSystemHealth::EmailIsEmpty:
        case QnSystemHealth::NoLicenses:
        case QnSystemHealth::SmtpIsNotSet:
        case QnSystemHealth::SystemIsReadOnly:
        case QnSystemHealth::RemoteArchiveSyncError:
        case QnSystemHealth::DefaultCameraPasswords:
        case QnSystemHealth::NoInternetForTimeSync:
        case QnSystemHealth::UsersEmailIsEmpty:
            return QnNotificationLevel::Value::ImportantNotification;

        // Red notifications.
        case QnSystemHealth::EmailSendError:
        case QnSystemHealth::ArchiveIntegrityFailed:
        case QnSystemHealth::StoragesNotConfigured:
            return QnNotificationLevel::Value::CriticalNotification;

        default:
            NX_ASSERT(false, "All cases must be handled here");
            break;
    }
    return QnNotificationLevel::Value::CriticalNotification;
}

QColor QnNotificationLevel::notificationColor(Value level)
{
    switch (level)
    {
        case Value::NoNotification:        return Qt::transparent;
        case Value::OtherNotification:     return Qt::white;
        case Value::CommonNotification:    return qnGlobals->notificationColorCommon();
        case Value::ImportantNotification: return qnGlobals->notificationColorImportant();
        case Value::CriticalNotification:  return qnGlobals->notificationColorCritical();
        case Value::SuccessNotification:   return qnGlobals->notificationColorCommon();
        default:
            NX_ASSERT(false, "All enum values must be handled");
            break;
    }
    return QColor();
}

QColor QnNotificationLevel::notificationTextColor(Value level)
{
    switch (level)
    {
        case Value::ImportantNotification: return qnGlobals->warningTextColor();
        case Value::CriticalNotification:  return qnGlobals->errorTextColor();
        case Value::SuccessNotification:   return qnGlobals->successTextColor();
        case Value::OtherNotification:     return Qt::white;
        default: return QColor(); //< Undefined and should be treated as default.
    }
}
