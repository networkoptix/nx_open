#include "notification_levels.h"

#include <utils/common/warnings.h>

#include <ui/style/globals.h>

Qn::NotificationLevel QnNotificationLevels::notificationLevel(BusinessEventType::Value eventType) {
    switch (eventType) {
    case BusinessEventType::Camera_Motion:
    case BusinessEventType::Camera_Input:
    case BusinessEventType::MediaServer_Started:
        return Qn::CommonNotification;      
    case BusinessEventType::Camera_Disconnect:
    case BusinessEventType::Storage_Failure:
    case BusinessEventType::Network_Issue:
        return Qn::ImportantNotification;
    case BusinessEventType::Camera_Ip_Conflict:
    case BusinessEventType::MediaServer_Failure:
    case BusinessEventType::MediaServer_Conflict:
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
        return Qn::CriticalNotification;
    }
}

QColor QnNotificationLevels::notificationColor(Qn::NotificationLevel level) {
    switch (level) {
    case Qn::NoNotification:        return Qt::transparent;
    case Qn::OtherNotification:     return Qt::white;
    case Qn::CommonNotification:    return qnGlobals->notificationColorCommon();
    case Qn::ImportantNotification: return qnGlobals->notificationColorImportant();
    case Qn::CriticalNotification:  return qnGlobals->notificationColorCritical();
    default:
        qnWarning("Invalid notification level '%1'.", static_cast<int>(level));
        return QColor();
    }
}
