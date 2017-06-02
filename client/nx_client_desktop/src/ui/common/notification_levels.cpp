#include "notification_levels.h"

#include <nx/vms/event/actions/abstract_action.h>

#include <utils/common/warnings.h>

#include <ui/style/globals.h>

using namespace nx;

QnNotificationLevel::Value QnNotificationLevel::valueOf(const vms::event::AbstractActionPtr &businessAction)
{
    if (businessAction->actionType() == vms::event::PlaySoundAction)
        return Value::CommonNotification;

    if (businessAction->actionType() == vms::event::ShowOnAlarmLayoutAction)
        return Value::CriticalNotification;

    auto params = businessAction->getRuntimeParams();
    vms::event::EventType eventType = params.eventType;

    if (eventType >= vms::event::UserDefinedEvent)
        return Value::CommonNotification;

    switch (eventType)
    {
        /* Green notifications */
        case vms::event::CameraMotionEvent:
        case vms::event::CameraInputEvent:
        case vms::event::ServerStartEvent:
        case vms::event::SoftwareTriggerEvent:
            return Value::CommonNotification;

        /* Yellow notifications */
        case vms::event::NetworkIssueEvent:
        case vms::event::CameraIpConflictEvent:
        case vms::event::ServerConflictEvent:
            return Value::ImportantNotification;

        /* Red notifications */
        case vms::event::CameraDisconnectEvent:
        case vms::event::StorageFailureEvent:
        case vms::event::ServerFailureEvent:
        case vms::event::LicenseIssueEvent:
            return Value::CriticalNotification;

        case vms::event::BackupFinishedEvent:
        {
            vms::event::EventReason reason = static_cast<vms::event::EventReason>(params.reasonCode);
            bool isCriticalNotification =
                reason == vms::event::BackupFailedChunkError ||
                reason == vms::event::BackupFailedNoBackupStorageError ||
                reason == vms::event::BackupFailedSourceFileError ||
                reason == vms::event::BackupFailedSourceStorageError ||
                reason == vms::event::BackupFailedTargetFileError;

            if (isCriticalNotification)
                return Value::CriticalNotification;

            return Value::CommonNotification;
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

        /* Green notifications */
        case QnSystemHealth::ArchiveRebuildFinished:
        case QnSystemHealth::ArchiveFastScanFinished: //this one is never displayed though
            return QnNotificationLevel::Value::CommonNotification;

        /* Yellow notifications */
        case QnSystemHealth::EmailIsEmpty:
        case QnSystemHealth::NoLicenses:
        case QnSystemHealth::SmtpIsNotSet:
        case QnSystemHealth::UsersEmailIsEmpty:
        case QnSystemHealth::NoPrimaryTimeServer:
        case QnSystemHealth::SystemIsReadOnly:
        case QnSystemHealth::StoragesNotConfigured:
        case QnSystemHealth::ArchiveRebuildCanceled:
            return QnNotificationLevel::Value::ImportantNotification;

        /* Red notifications */
        case QnSystemHealth::EmailSendError:
        case QnSystemHealth::StoragesAreFull:
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
        default:
            NX_ASSERT(false, Q_FUNC_INFO, "All enum values must be handled");
            break;
    }
    return QColor();
}
