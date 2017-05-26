#include "notification_levels.h"

#include <business/actions/abstract_business_action.h>

#include <utils/common/warnings.h>

#include <ui/style/globals.h>

QnNotificationLevel::Value QnNotificationLevel::valueOf(const QnAbstractBusinessActionPtr &businessAction)
{
    if (businessAction->actionType() == QnBusiness::PlaySoundAction)
        return Value::CommonNotification;

    if (businessAction->actionType() == QnBusiness::ShowOnAlarmLayoutAction)
        return Value::CriticalNotification;

    auto params = businessAction->getRuntimeParams();
    QnBusiness::EventType eventType = params.eventType;

    if (eventType >= QnBusiness::UserDefinedEvent)
        return Value::CommonNotification;

    switch (eventType)
    {
        /* Green notifications */
        case QnBusiness::CameraMotionEvent:
        case QnBusiness::CameraInputEvent:
        case QnBusiness::ServerStartEvent:
        case QnBusiness::SoftwareTriggerEvent:
            return Value::CommonNotification;

        /* Yellow notifications */
        case QnBusiness::NetworkIssueEvent:
        case QnBusiness::CameraIpConflictEvent:
        case QnBusiness::ServerConflictEvent:
            return Value::ImportantNotification;

        /* Red notifications */
        case QnBusiness::CameraDisconnectEvent:
        case QnBusiness::StorageFailureEvent:
        case QnBusiness::ServerFailureEvent:
        case QnBusiness::LicenseIssueEvent:
            return Value::CriticalNotification;

        case QnBusiness::BackupFinishedEvent:
        {
            QnBusiness::EventReason reason = static_cast<QnBusiness::EventReason>(params.reasonCode);
            bool isCriticalNotification =
                reason == QnBusiness::BackupFailedChunkError ||
                reason == QnBusiness::BackupFailedNoBackupStorageError ||
                reason == QnBusiness::BackupFailedSourceFileError ||
                reason == QnBusiness::BackupFailedSourceStorageError ||
                reason == QnBusiness::BackupFailedTargetFileError;

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
