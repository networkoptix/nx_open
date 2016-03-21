#include "notification_levels.h"

#include <business/actions/abstract_business_action.h>

#include <utils/common/warnings.h>

#include <ui/style/globals.h>

QnNotificationLevel::Value QnNotificationLevel::valueOf(const QnAbstractBusinessActionPtr &businessAction) {

    if (businessAction->actionType() == QnBusiness::PlaySoundAction) 
        return Value::CommonNotification;

    if (businessAction->actionType() == QnBusiness::ShowOnAlarmLayoutAction) 
        return Value::CriticalNotification;
    
    auto params = businessAction->getRuntimeParams();
    QnBusiness::EventType eventType = params.eventType;

    if (eventType >= QnBusiness::UserDefinedEvent)
        return Value::CommonNotification;

    switch (eventType) {
    case QnBusiness::CameraMotionEvent:
    case QnBusiness::CameraInputEvent:
    case QnBusiness::ServerStartEvent:
        return Value::CommonNotification;

    case QnBusiness::BackupFinishedEvent:
        {
            QnBusiness::EventReason reason = static_cast<QnBusiness::EventReason>(params.reasonCode);
            bool isCriticalNotification = reason == QnBusiness::BackupFailedChunkError || 
                                          reason == QnBusiness::BackupFailedNoBackupStorageError ||
                                          reason == QnBusiness::BackupFailedSourceFileError ||
                                          reason == QnBusiness::BackupFailedSourceStorageError ||
                                          reason == QnBusiness::BackupFailedTargetFileError;
            if (isCriticalNotification)
                return Value::CriticalNotification;
            return Value::CommonNotification;
        }

    case QnBusiness::CameraDisconnectEvent:
    case QnBusiness::StorageFailureEvent:
    case QnBusiness::NetworkIssueEvent:
        return Value::ImportantNotification;

    case QnBusiness::CameraIpConflictEvent:
    case QnBusiness::ServerFailureEvent:
    case QnBusiness::ServerConflictEvent:
    case QnBusiness::LicenseIssueEvent:
        return Value::CriticalNotification;

    default:                                        
        Q_ASSERT_X(false, Q_FUNC_INFO, "All enum values must be handled");
        return Value::NoNotification;
    }
}

QnNotificationLevel::Value QnNotificationLevel::valueOf(QnSystemHealth::MessageType messageType) {
    switch (messageType) {
    case QnSystemHealth::ArchiveRebuildFinished:
    case QnSystemHealth::ArchiveRebuildCanceled:
        return QnNotificationLevel::Value::CommonNotification;
    default:
        break;
    }
    return QnNotificationLevel::Value::SystemNotification;
}

QColor QnNotificationLevel::notificationColor(Value level) {
    switch (level) {
    case Value::NoNotification:        return Qt::transparent;
    case Value::OtherNotification:     return Qt::white;
    case Value::CommonNotification:    return qnGlobals->notificationColorCommon();
    case Value::ImportantNotification: return qnGlobals->notificationColorImportant();
    case Value::CriticalNotification:  return qnGlobals->notificationColorCritical();
    case Value::SystemNotification:    return qnGlobals->notificationColorSystem();
    default:
        Q_ASSERT_X(false, Q_FUNC_INFO, "All enum values must be handled");
        break;        
    }
    return QColor();
}
