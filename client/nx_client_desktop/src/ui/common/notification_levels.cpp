#include "notification_levels.h"

#include <nx/vms/event/actions/abstract_action.h>

#include <utils/common/warnings.h>

#include <ui/style/globals.h>

using namespace nx;

QnNotificationLevel::Value QnNotificationLevel::valueOf(const vms::event::AbstractActionPtr &businessAction)
{
    if (businessAction->actionType() == vms::event::playSoundAction)
        return Value::CommonNotification;

    if (businessAction->actionType() == vms::event::showOnAlarmLayoutAction)
        return Value::CriticalNotification;

    auto params = businessAction->getRuntimeParams();
    vms::event::EventType eventType = params.eventType;

    if (eventType >= vms::event::userDefinedEvent)
        return Value::CommonNotification;

    switch (eventType)
    {
        // Gray notifications.
        case vms::event::cameraMotionEvent:
        case vms::event::cameraInputEvent:
        case vms::event::serverStartEvent:
        case vms::event::softwareTriggerEvent:
        case vms::event::analyticsSdkEvent:
            return Value::CommonNotification;

        // Yellow notifications.
        case vms::event::networkIssueEvent:
        case vms::event::cameraIpConflictEvent:
        case vms::event::serverConflictEvent:
            return Value::ImportantNotification;

        // Red notifications.
        case vms::event::cameraDisconnectEvent:
        case vms::event::storageFailureEvent:
        case vms::event::serverFailureEvent:
        case vms::event::licenseIssueEvent:
            return Value::CriticalNotification;

        case vms::event::backupFinishedEvent:
        {
            vms::event::EventReason reason = static_cast<vms::event::EventReason>(params.reasonCode);
            const bool failure =
                reason == vms::event::EventReason::backupFailedChunkError ||
                reason == vms::event::EventReason::backupFailedNoBackupStorageError ||
                reason == vms::event::EventReason::backupFailedSourceFileError ||
                reason == vms::event::EventReason::backupFailedSourceStorageError ||
                reason == vms::event::EventReason::backupFailedTargetFileError;

            if (failure)
                return Value::CriticalNotification;

            const bool success = reason == vms::event::EventReason::backupDone;
            return success ? Value::SuccessNotification : Value::CommonNotification;
        }

        default:
            NX_ASSERT(false, Q_FUNC_INFO, "All enum values must be handled");
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
        case QnSystemHealth::UsersEmailIsEmpty:
        case QnSystemHealth::SystemIsReadOnly:
        case QnSystemHealth::StoragesNotConfigured:
        case QnSystemHealth::RemoteArchiveSyncError:
            return QnNotificationLevel::Value::ImportantNotification;

        // Red notifications.
        case QnSystemHealth::EmailSendError:
        case QnSystemHealth::ArchiveIntegrityFailed:
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
            NX_ASSERT(false, Q_FUNC_INFO, "All enum values must be handled");
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
