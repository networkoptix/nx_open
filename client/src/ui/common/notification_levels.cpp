#include "notification_levels.h"

#include <utils/common/warnings.h>

#include <ui/style/globals.h>

Qn::NotificationLevel QnNotificationLevels::notificationLevel(QnBusiness::EventType eventType) {
    switch (eventType) {
    case QnBusiness::CameraMotionEvent:
    case QnBusiness::CameraInputEvent:
    case QnBusiness::ServerStartEvent:
        return Qn::CommonNotification;      
    case QnBusiness::CameraDisconnectEvent:
    case QnBusiness::StorageFailureEvent:
    case QnBusiness::NetworkIssueEvent:
        return Qn::ImportantNotification;
    case QnBusiness::CameraIpConflictEvent:
    case QnBusiness::ServerFailureEvent:
    case QnBusiness::ServerConflictEvent:
    case QnBusiness::LicenseIssueEvent:
        return Qn::CriticalNotification;
    default:                                        
        qnWarning("Invalid business event type '%1'.", static_cast<int>(eventType));
        return Qn::NoNotification;
    }
}

Qn::NotificationLevel QnNotificationLevels::notificationLevel(QnSystemHealth::MessageType messageType) {
    switch (messageType) {
    case QnSystemHealth::ArchiveRebuildFinished:
        return Qn::CommonNotification;
    default:
        return Qn::SystemNotification;
    }
}

QColor QnNotificationLevels::notificationColor(Qn::NotificationLevel level) {
    switch (level) {
    case Qn::NoNotification:        return Qt::transparent;
    case Qn::OtherNotification:     return Qt::white;
    case Qn::CommonNotification:    return qnGlobals->notificationColorCommon();
    case Qn::ImportantNotification: return qnGlobals->notificationColorImportant();
    case Qn::CriticalNotification:  return qnGlobals->notificationColorCritical();
    case Qn::SystemNotification:    return qnGlobals->notificationColorSystem();
    default:
        qnWarning("Invalid notification level '%1'.", static_cast<int>(level));
        return QColor();
    }
}
